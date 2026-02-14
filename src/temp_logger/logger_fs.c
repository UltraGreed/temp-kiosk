/// This logger attempts to use a ring-buffer-like logic for log 1:
/// if the first entry in the log is older than maximum allowed period of time,
/// it starts to rewrite log from there line by line, until it reaches end of file.
///
/// Whenever the end of file is reached, first entry of the log is checked on timeout again.
/// At the end of the execution all the lines in the logger are shifted to be sorted by date.
///
/// Logs 2 and 3 are sorted after each write.

#include "logger_interface.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cross_time.h"
#include "my_types.h"
#include "temp_logger.h"
#include "utils.h"

// Including \n
#define LOG_LINE_LEN (DATE_LEN + MSG_LEN + 3)

#define READ_BUF_SIZE 1024

struct Log {
    FILE *file;

    f64 first_entry_time;
};

static int swap_file_parts(FILE *file);
static f64 first_entry_secs(FILE *file);
static int get_value_precision(f64 value);
static void print_value(char *str, f64 value);

Log *init_log(const char log_dir[], const char log_file[])
{
    char *log_path = join_paths_xmalloc(log_dir, log_file);

    Log *log = xmalloc(sizeof(Log));

    // Create log without overwriting it if it exists.
    // "a+" is the only way to do so, but it doesn't allow to overwrite written data later,
    log->file = xfopen(log_path, "a+");
    fclose(log->file);
    // so we have to reopen file with "r+".
    log->file = xfopen(log_path, "rb+");

    if (fsize(log->file) == 0) {
        log->first_entry_time = -INFINITY;
        return log;
    }

    /// Attempt to parse log
    f64 first = first_entry_secs(log->file);
    if (first == INFINITY) {
        fprintf(stderr, "Failed to parse date from log %s. Overwriting it.\n", log_path);
        log->first_entry_time = -INFINITY;
    } else {
        log->first_entry_time = first;
    }

    free(log_path);
    return log;
}

int deinit_log(Log *log)
{
    int res1 = swap_file_parts(log->file);
    if (res1 == -1)
        perror("Failed to sort log on exit");

    int res2 = fclose(log->file);
    if (res2 == -1)
        perror("Failed to close log on exit");

    free(log);

    return (res1 == 0 && res2 == 0) ? 0 : -1;
}

int write_log(Log *log, f64 value, DateTime *date, usize max_period)
{
    time_t secs = to_secs(date);
    int at_end = fatend(log->file);
    if (at_end == -1) {
        fprintf(stderr, "No longer able to access log file! %s (%d)\n", strerror(errno), errno);
        return -1;
    }
    if (at_end && secs - log->first_entry_time > max_period) {
        rewind(log->file);
        log->first_entry_time = secs;
    }

    char date_str[DATE_LEN + 1];
    print_date(date_str, date);

    char value_str[MSG_LEN]; // No +1 because we don't need a delimiter
    print_value(value_str, value);

    // printf("LOGGED: %s : %s\n", date_str, value_str);
    fprintf(log->file, "%s : %s\n", date_str, value_str);
    fflush(log->file);

    return 0;
}

f64 get_avg_log(Log *log, f64 period, DateTime *date)
{
    i64 start_pos = ftello(log->file);
    if (start_pos == -1)
        return INFINITY;
    rewind(log->file);

    f64 sum = 0;
    usize ctr = 0;

    char line_buf[LOG_LINE_LEN + 1];
    while (fgets(line_buf, LOG_LINE_LEN + 1, log->file) != NULL) {
        DateTime date_entry;
        int res = scan_date(line_buf, &date_entry);

        f64 t = to_secs(&date_entry);
        if (res == -1 || t == INFINITY) {
            fprintf(stderr, "Incorrect date found in the log file!: \"%s\"\n", line_buf);
            continue;
        }

        if (to_secs(date) - t <= period) {
            f64 val;
            sscanf(line_buf, "%*s %*s : %lf", &val);
            sum += val;
            ctr++;
        }
    }

    if (ctr == 0) {
        fprintf(stderr, "Failed to calculate average of period: no matching entries!\n");
        return INFINITY;
    }

    fseeko(log->file, start_pos, SEEK_SET);

    return sum / ctr;
}

int delete_old_entries(Log *log, DateTime *date, usize max_period)
{
    assert(log->file != NULL);
    rewind(log->file);

    i64 write_pos = 0;
    char buf[LOG_LINE_LEN + 1];
    while (!fatend(log->file)) {
        fgets(buf, LOG_LINE_LEN + 1, log->file);
        char *delim_pos = strstr(buf, "\n");
        if (&buf[LOG_LINE_LEN - 1] != delim_pos) {
            fseeko(log->file, -strlen(buf), SEEK_CUR);
            char ch;
            do {
                ch = fgetc(log->file);
            } while (ch != '\n' && ch != EOF);
            continue;
        }

        DateTime date_entry;
        int resd = scan_date(buf, &date_entry);
        if (resd == 0 && to_secs(date) - to_secs(&date_entry) <= max_period) {
            i64 read_pos = ftello(log->file);

            fseeko(log->file, write_pos, SEEK_SET);
            fwrite(buf, sizeof(char), LOG_LINE_LEN, log->file);
            write_pos = ftello(log->file);

            fseeko(log->file, read_pos, SEEK_SET);
        }
    }

    if (write_pos != ftello(log->file)) {
        ftrunc(log->file, write_pos);
        freopen(NULL, "r+", log->file);
        fseeko(log->file, 0, SEEK_END);
    }

    if (fsize(log->file) > 0)
        log->first_entry_time = first_entry_secs(log->file);

    return 0;
}

/// Inplace swap all the data before the current file pos and after the current file pos.
/// Is used to sort file ring buffer.
/// Return 0 on success, -1 otherwise.
static int swap_file_parts(FILE *file)
{
    int at_end = fatend(file);
    if (at_end == -1) {
        perror("Failed to access file file");
        return -1;
    }

    if (at_end)
        return 0;

    i64 n_left = ftello(file);

    // TODO: it would be better to check which part is smaller and alloc for it
    char *swp_buf = xmalloc(n_left * sizeof(char));

    rewind(file);
    fread(swp_buf, sizeof(char), n_left, file);

    // Write right part to the beginning
    while (!fatend(file)) {
        char rw_buf[READ_BUF_SIZE];
        int n_read = fread(rw_buf, sizeof(char), READ_BUF_SIZE, file);
        assert(n_read != 0);

        fseeko(file, -n_left - n_read, SEEK_CUR);
        fwrite(rw_buf, sizeof(char), n_read, file);
        fseeko(file, n_left, SEEK_CUR);
    }

    // Write left part from buffer
    fseeko(file, -n_left, SEEK_END);
    fwrite(swp_buf, sizeof(char), n_left, file);
    fflush(file);

    free(swp_buf);

    return 0;
}

/// Return time from the first line of the log, -1 on error.
static f64 first_entry_secs(FILE *file)
{
    assert(file != NULL);
    i64 fpos = ftello(file);
    rewind(file);

    char buf[LOG_LINE_LEN + 1];
    void *res = fgets(buf, LOG_LINE_LEN + 1, file);
    assert(res != NULL);

    DateTime date;
    int resd = scan_date(buf, &date);
    if (resd != 0)
        return INFINITY;

    fseeko(file, fpos, SEEK_SET);
    return to_secs(&date);
}

static int get_value_precision(f64 value)
{
    usize len = flen(value);
    if (len < MSG_LEN - 2)
        return MSG_LEN - len - 2;
    else
        return 0;
}

static void print_value(char *str, f64 value)
{
    int res = snprintf(str, MSG_LEN, "%.*lf", get_value_precision(value), value);
    assert(res == MSG_LEN - 1);
}

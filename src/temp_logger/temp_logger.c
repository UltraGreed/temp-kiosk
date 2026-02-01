/// This logger attempts to use a ring-buffer-like logic for log 1:
/// if the first entry in the log is older than maximum allowed period of time,
/// it starts to rewrite log from there line by line, until it reaches end of file.
///
/// Whenever the end of file is reached, first entry of the log is checked on timeout again.
/// At the end of the execution all the lines in the logger are shifted to be sorted by date.
///
/// Logs 2 and 3 are sorted after each write.

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "cross_time.h"
#include "utils/files_utils.h"
#include "utils/str_utils.h"

#define DELIM '\n'
#define MSG_LEN 8
#define LINE_LEN 29

#define DATE_FMT "%Y-%m-%d %H:%M:%S"
#define DATE_LEN 19

#define MAX_KEEP_LOG1 20
#define MAX_KEEP_LOG2 60
#define MAX_KEEP_LOG3 150
#define PERIOD_LOG2 10
#define PERIOD_LOG3 30

#define RW_BUF_SIZE 1024

static bool is_working = true;
void sigint_handler(int sig) {
    (void)sig;
    is_working = false;
}

/// Date in log is represented as a ring buffer.
/// This function tries to find first date
/// Return -1 on error, 0 if found date loop, 1 otherwise.
// int seek_first_date(FILE *file) {
//     char line_buf[LINE_LEN + 1];
//     struct tm prev_tm;
//     struct tm tm;
//
//     bool first = true;
//     while (fgets(line_buf, LINE_LEN + 1, file) != NULL) {
//         int res = scan_date(line_buf, &tm);
//         if (res != 0) {
//             perror("Failed to parse log file (possibly wrong format) errno:");
//             return -1;
//         }
//
//         if (!first && difftime(mktime(&prev_tm), mktime(&tm)) < 0) {
//             // The log was cycled here
//             res = fseek(file, -LINE_LEN, SEEK_CUR);
//             assert(res == 0);
//             return 0;
//         };
//
//         first = false;
//     }
// }

/// Return time from the first line of the log, -1 on error.
time_t get_first_time(FILE *log) {
    int fpos = ftell(log);
    assert(fpos != -1);
    rewind(log);

    char buf[LINE_LEN + 1];
    void *res = fgets(buf, LINE_LEN + 1, log);
    assert(res != NULL);

    struct tm first_date;
    int resd = scan_date(buf, &first_date);
    assert(resd != -1);

    fseek(log, fpos, SEEK_SET);
    return mktime(&first_date);
}

/// Swaps all the data before current file pos and after current file pos.
/// Return 0 on success, -1 otherwise.
int sort_log(FILE *log) {
    if (fatend(log)) 
        return 0;

    int n_left = ftell(log);
    if (n_left == -1)
        return -1;
    
    // TODO: it would be better to check which part is smaller and alloc for it
    char *swp_buf = malloc(n_left * sizeof(char));
    if (swp_buf == NULL) {
        perror("Failed malloc for sorting log");
        return -1;
    }

    rewind(log);
    fread(swp_buf, sizeof(char), n_left, log);

    // Write right part to the beginning
    while (!fatend(log)) {
        char rw_buf[RW_BUF_SIZE];
        int n_read = fread(rw_buf, sizeof(char), RW_BUF_SIZE, log);
        assert(n_read != 0);

        fseek(log, -n_left - n_read, SEEK_CUR);
        fwrite(rw_buf, sizeof(char), n_read, log);
        fseek(log, n_left, SEEK_CUR);
    }

    // Write left part from buffer
    fseek(log, -n_left, SEEK_END);
    fwrite(swp_buf, sizeof(char), n_left, log);
    fflush(log);

    free(swp_buf);

    return 0;
}

/// Return average for given period of time from log file.
f64 get_log_period_avg(FILE *log, time_t period) {
    int start_pos = ftell(log);
    rewind(log);

    time_t start_t = time(NULL);
    char line_buf[LINE_LEN + 1];
    struct tm tm;

    while (fgets(line_buf, LINE_LEN + 1, log) != NULL) {
        int res = scan_date(line_buf, &tm);
        assert(res == 0);

        if (difftime(mktime(&tm), start_t) <= period) {
            fseek(log, -LINE_LEN, SEEK_CUR);
            break;
        }
    }

    assert(!fatend(log));

    f64 sum = 0;
    usize ctr = 0;
    while (fgets(line_buf, LINE_LEN + 1, log) != NULL) {
        f64 val;
        sscanf(line_buf, "%*s %*s : %lf", &val);

        sum += val;
        ctr++;
    }

    fseek(log, start_pos, SEEK_SET);

    return sum / ctr;
}

/// Attempt to write value to log.
/// Return -1 on error, 0 on success.
int write_log(const char *log_name, const char *date_str, f64 value, i32 max_keep) {
    FILE *log = fopen(log_name, "r+");
    if (log == NULL) {
        fprintf(stderr, "File %s no longer available! %s (%d)\n Ignoring...\n", log_name, strerror(errno),
                errno);
        return -1;
    }
    rewind(log);

    // If log exists, but the first entry is either damaged or too old, rewrite it.
    // Continue from the end otherwise.
    if (fsize(log) > 0) {
        time_t first_time = get_first_time(log);
        if (first_time == -1)
            fprintf(stderr, "Failed to parse date from %s. Overwriting it.\n", log_name);
        else if (difftime(first_time, time(NULL)) <= max_keep)
            fseek(log, 0, SEEK_END);
    }

    i32 val_width = MSG_LEN - (i32)flen(value) - 2;

    printf("%s : %.*lf\n", date_str, val_width > 0 ? val_width : 0, value);
    fprintf(log, "%s : %.*lf\n", date_str, val_width > 0 ? val_width : 0, value);
    fflush(log);

    sort_log(log);
    fclose(log);

    return 0;
}

const char *log1_name = "log1.txt";
const char *log2_name = "log2.txt";
const char *log3_name = "log3.txt";

int main(int argc, char *argv[]) {
    i64 res;
    if (argc != 2) {
        fprintf(stderr, "Usage: temp_logger DEVICE\n");
        exit(2);
    }

    signal(SIGINT, sigint_handler);

    const char *dev_name = argv[1];

    FILE *dev_file = fopen_or_exit(dev_name, "r");

    FILE *log1 = fopen_or_exit(log1_name, "a+");
    fclose(log1);
    // We don't need them to be always open,
    // but it's nice to check that path is correct inbefore.
    FILE *log2 = fopen_or_exit(log2_name, "a+");
    fclose(log2);
    FILE *log3 = fopen_or_exit(log3_name, "a+");
    fclose(log3);

    log1 = fopen_or_exit(log1_name, "w+");

    time_t start_t = time(NULL);
    time_t log1_first_time;
    if (fsize(log1) == 0) {
        log1_first_time = start_t;
    } else {
        // Log 1 already exists
        log1_first_time = get_first_time(log1);
        printf("DIFF TIME %f\n", difftime(start_t, log1_first_time));
        if (log1_first_time == -1) {
            perror("Failed to parse date from log 1. Overwriting it");
            fclose(log1);
            log1 = fopen_or_exit(log1_name, "w+");
        }
        if (log1_first_time == -1 || difftime(start_t, log1_first_time) > MAX_KEEP_LOG1) {
            log1_first_time = start_t;
        } else {
             // Start appending at the end if existing log is not too old.
            fseek(log1, 0, SEEK_END);
        }
    }

    time_t log2_last_time = start_t;
    time_t log3_last_time = start_t;

    // Skip all bytes till delim and all old data from serial
    time_t skip_time = time(NULL);
    while (fgetc(dev_file) != DELIM || time(NULL) == skip_time) {}

    while (is_working) {
        char temp_str[MSG_LEN + 1];
        res = (i64)fgets(temp_str, MSG_LEN + 1, dev_file);
        if (res == (i64)NULL) {
            fprintf(stderr, "ERROR: Failed to read from %s: %s (%d)\n", dev_name, strerror(errno), errno);
            continue;
        }

        if (temp_str[MSG_LEN - 1] != DELIM) {
            fprintf(stderr, "Damaged message length from device %s\n", dev_name);
            while (fgetc(dev_file) != DELIM) {} // Skip all bytes till delim (including)
            continue;
        }

        f64 value;
        if (sscanf(temp_str, "%lf", &value) != 1) {
            fprintf(stderr, "Incorrect input from device %s\n", dev_name);
            continue;
        }

        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        char date_str[DATE_LEN + 1];
        res = strftime(date_str, DATE_LEN + 1, "%Y-%m-%d %H:%M:%S", tm);
        assert(res == DATE_LEN);

        if (fatend(log1) && difftime(t, log1_first_time) > MAX_KEEP_LOG1) {
            rewind(log1);
            log1_first_time = t;
        }

        printf("%s : %.*s\n", date_str, MSG_LEN - 1, temp_str);
        fprintf(log1, "%s : %.*s\n", date_str, MSG_LEN - 1, temp_str);
        fflush(log1);

        if (difftime(t, log2_last_time) >= PERIOD_LOG2) {
            sort_log(log1);
            log1_first_time = get_first_time(log1);
            f64 avg = get_log_period_avg(log1, PERIOD_LOG2);
            write_log(log2_name, date_str, avg, MAX_KEEP_LOG2);
            log2_last_time = t;
        }

        if (difftime(t, log3_last_time) >= PERIOD_LOG3) {
            f64 avg = get_log_period_avg(log2, PERIOD_LOG3);
            write_log(log3_name, date_str, avg, MAX_KEEP_LOG3);
            log3_last_time = t;
        }
    }

    sort_log(log1);
    fclose(log1);

    fclose(dev_file);

    printf("Log writing finished successfully.\n");

    return 0;
}

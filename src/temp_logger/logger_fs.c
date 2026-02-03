#include "logger_interface.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cross_time.h"
#include "temp_logger.h"
#include "utils/file_utils.h"

#define LINE_LEN (DATE_LEN + MSG_LEN + 3)  // Including \n
#define LOG1_NAME "log1.txt"
#define LOG2_NAME "log2.txt"
#define LOG3_NAME "log3.txt"

#define RW_BUF_SIZE 1024

struct Logger {
    FILE *log1;

    char *log1_path;
    char *log2_path;
    char *log3_path;

    f64 log1_first_time;
};

static int swap_file_parts(FILE *log);
static f64 _get_log_avg(FILE *log, f64 period, f64 secs);
static f64 first_entry_secs(FILE *log);
static void try_create_logs(char *path1, char *path2, char *path3);
static int _write_log(const char *log_name, f64 value, DateTime *date, i32 max_keep);
static int get_value_precision(f64 value);
static int print_value(char *str, f64 value);

Logger *create_logger(char **params, f64 secs)
{
    Logger *logger = malloc(sizeof(Logger));

    const char *dir_name = params[0];
    const int dir_len = strlen(dir_name);
    logger->log1_path = malloc(sizeof(char) * (dir_len + strlen(LOG1_NAME) + 2));
    logger->log2_path = malloc(sizeof(char) * (dir_len + strlen(LOG2_NAME) + 2));
    logger->log3_path = malloc(sizeof(char) * (dir_len + strlen(LOG3_NAME) + 2));

    sprintf(logger->log1_path, "%s/%s", dir_name, LOG1_NAME);
    sprintf(logger->log2_path, "%s/%s", dir_name, LOG2_NAME);
    sprintf(logger->log3_path, "%s/%s", dir_name, LOG3_NAME);

    // Check if logs are accessible and exit right away if they are not.
    try_create_logs(logger->log1_path, logger->log2_path, logger->log3_path);

    logger->log1 = fopen(logger->log1_path, "r+");
    assert(logger->log1 != NULL);

    if (fsize(logger->log1) == 0)
        return logger;

    /// Attempt to parse log1
    f64 first = first_entry_secs(logger->log1);
    if (first == INFINITY)
        fprintf(stderr, "Failed to parse date from log 1. Overwriting it.\n");

    bool is_old = (secs - first) > MAX_KEEP_LOG1;
    if (first == INFINITY || is_old) {
        // Overwrite log
        fclose(logger->log1);
        logger->log1 = fopen(logger->log1_path, "w+");
        logger->log1_first_time = secs;
    } else {
        // Continue log
        fseek(logger->log1, 0, SEEK_END);
        logger->log1_first_time = first;
    }

    return logger;
}

int deinit_logger(Logger *logger)
{
    free(logger->log1_path);
    free(logger->log2_path);
    free(logger->log3_path);

    int res1 = swap_file_parts(logger->log1);
    if (res1 == -1)
        perror("Failed to sort log 1 on exit");

    int res2 = fclose(logger->log1);
    if (res2 == -1)
        perror("Failed to close log 1 on exit");

    return (res1 == 0 && res2 == 0) ? 0 : -1;
}

f64 get_avg_log1(Logger *logger, f64 period, f64 secs)
{
    f64 avg = _get_log_avg(logger->log1, period, secs);
    if (avg == INFINITY)
        perror("Error occured in log1 file!");
    return avg;
}

f64 get_avg_log2(Logger *logger, f64 period, f64 secs)
{
    FILE *log = fopen(logger->log2_path, "r");
    if (log == NULL) {
        perror("Failed to access log2 file!");
        return INFINITY;
    }

    f64 avg = _get_log_avg(log, period, secs);
    if (avg == INFINITY)
        perror("Error occured in log2 file!");
    return avg;
}

int write_log1(Logger *logger, f64 value, DateTime *date)
{
    time_t secs = to_secs(date);
    if (fatend(logger->log1) && secs - logger->log1_first_time > MAX_KEEP_LOG1) {
        rewind(logger->log1);
        logger->log1_first_time = secs;
    }

    char date_str[DATE_LEN + 1];
    int res = print_date(date_str, date);
    assert(res == 0);

    char value_str[MSG_LEN];
    res = print_value(value_str, value);
    assert(res == 0);

    printf("LOG1: %s : %s\n", date_str, value_str);
    fprintf(logger->log1, "%s : %s\n", date_str, value_str);
    fflush(logger->log1);

    return 0;
}

int write_log2(Logger *logger, f64 avg, DateTime *date)
{
    int res = _write_log(logger->log2_path, avg, date, MAX_KEEP_LOG2);
    if (res == -1)
        fprintf(stderr, "Failed to write log 2\n");
    else
        fprintf(stderr, "Written to log 2 successfully\n");
    return res;
}

int write_log3(Logger *logger, f64 avg, DateTime *date)
{
    int res = _write_log(logger->log3_path, avg, date, MAX_KEEP_LOG3);
    if (res == -1)
        fprintf(stderr, "Failed to write log 3\n");
    else
        fprintf(stderr, "Written to log 3 successfully\n");
    return res;
}

/// Swaps all the data before current file pos and after current file pos.
/// Return 0 on success, -1 otherwise.
static int swap_file_parts(FILE *log)
{
    if (fatend(log))
        return 0;

    int n_left = ftell(log);
    if (n_left == -1) {
        perror("Failed to access log file");
        return -1;
    }

    // TODO: it would be better to check which part is smaller and alloc for it
    char *swp_buf = malloc(n_left * sizeof(char));
    if (swp_buf == NULL) {
        perror("Failed to malloc for sorting log");
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

static f64 _get_log_avg(FILE *log, f64 period, f64 secs)
{
    int start_pos = ftell(log);
    rewind(log);

    f64 sum = 0;
    usize ctr = 0;

    char line_buf[LINE_LEN + 1];
    while (fgets(line_buf, LINE_LEN + 1, log) != NULL) {
        DateTime date_entry;
        int res = scan_date(line_buf, &date_entry);

        f64 t = to_secs(&date_entry);
        if (res == -1 || t == INFINITY) {
            perror("Incorrect date found in log file!");
            continue;
        }

        if (secs - t <= period) {
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

    fseek(log, start_pos, SEEK_SET);

    return sum / ctr;
}

/// Return time from the first line of the log, -1 on error.
static f64 first_entry_secs(FILE *log)
{
    int fpos = ftell(log);
    if (fpos == -1) {
        perror("Failed to access log");
        exit(1);
    };
    rewind(log);

    char buf[LINE_LEN + 1];
    void *res = fgets(buf, LINE_LEN + 1, log);
    assert(res != NULL);

    DateTime date;
    int resd = scan_date(buf, &date);
    if (resd != 0)
        return INFINITY;

    fseek(log, fpos, SEEK_SET);
    return to_secs(&date);
}

static void try_create_logs(char *path1, char *path2, char *path3)
{
    FILE *f1 = fopen_or_exit(path1, "a+");
    fclose(f1);
    FILE *f2 = fopen_or_exit(path2, "a+");
    fclose(f2);
    FILE *f3 = fopen_or_exit(path3, "a+");
    fclose(f3);
}

static int _write_log(const char *log_name, f64 value, DateTime *date, i32 max_keep)
{
    FILE *log = fopen(log_name, "r+");
    if (log == NULL) {
        fprintf(stderr, "File %s no longer available! %s (%d)\n Skipping write...\n", log_name,
                strerror(errno), errno);
        return -1;
    }

    // If log exists, but is damaged, rewrite it completely.
    // If log exists, but is old, rewrite it line by line.
    // Continue from the end otherwise.
    if (fsize(log) > 0) {
        f64 first = first_entry_secs(log);

        if (first == INFINITY) {
            fprintf(stderr, "Failed to parse date from %s. Overwriting it.\n", log_name);
            fclose(log);
            log = fopen(log_name, "w+");
        } else if (to_secs(date) - first > max_keep) {
            fseek(log, 0, SEEK_SET);
        }
    }

    char value_str[MSG_LEN];
    int res = print_value(value_str, value);
    assert(res == 0);

    char date_str[DATE_LEN + 1];
    int resd = print_date(date_str, date);
    assert(resd == 0);

    fprintf(log, "%s : %s\n", date_str, value_str);
    fflush(log);

    swap_file_parts(log);
    fclose(log);

    return 0;
}

static int get_value_precision(f64 value)
{
    usize len = flen(value);
    if (len < MSG_LEN - 2)
        return MSG_LEN - len - 2;
    else
        return 0;
}

static int print_value(char *str, f64 value)
{
    int res = snprintf(str, MSG_LEN, "%.*lf", get_value_precision(value), value);
    if (res != MSG_LEN - 1)
        return -1;
    return 0;
}

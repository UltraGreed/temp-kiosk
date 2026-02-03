/// This logger attempts to use a ring-buffer-like logic for log 1:
/// if the first entry in the log is older than maximum allowed period of time,
/// it starts to rewrite log from there line by line, until it reaches end of file.
///
/// Whenever the end of file is reached, first entry of the log is checked on timeout again.
/// At the end of the execution all the lines in the logger are shifted to be sorted by date.
///
/// Logs 2 and 3 are sorted after each write.

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "cross_time.h"
#include "logger_interface.h"
#include "temp_logger.h"
#include "utils/file_utils.h"
#include "utils/str_utils.h"

static bool is_working = true;
void sigint_handler(int sig)
{
    (void)sig;
    is_working = false;
}

void skip_msg(FILE *dev_file)
{
    while (fgetc(dev_file) != DELIM) {};
}

void skip_old_msg(FILE *dev_file)
{
    time_t t = time(NULL);
    while (t == time(NULL))
        fgetc(dev_file);
}

/// Read value from device.
/// Return 0 on success, -1 on error;
int read_value(FILE *dev, f64 *value)
{
    char temp_str[MSG_LEN + 1];
    void *res = fgets(temp_str, MSG_LEN + 1, dev);
    if (res == NULL) {
        perror("Failed to read from device");
        return -1;
    }

    if (temp_str[MSG_LEN - 1] != DELIM) {
        perror("Damaged message length from device");
        skip_msg(dev); // Skip all bytes till delim (including)
        return -1;
    }

    f64 new_val;
    if (sscanf(temp_str, "%lf", &new_val) != 1) {
        fprintf(stderr, "Incorrect input from device: %s\n", temp_str);
        return -1;
    }

    *value = new_val;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: temp_logger DEVICE LOG_PATH (db or dir)\n");
        exit(2);
    }
    signal(SIGINT, sigint_handler);

    const char *dev_name = argv[1];
    FILE *dev_file = fopen_or_exit(dev_name, "r");

    Logger *logger = create_logger(&argv[2], get_secs());

    skip_old_msg(dev_file);
    skip_msg(dev_file);

    f64 log2_last_write = get_secs();
    f64 log3_last_write = log2_last_write;

    while (is_working) {
        f64 value;
        read_value(dev_file, &value);

        f64 secs = get_secs();
        assert(secs != INFINITY);
        DateTime date;
        int res = get_datetime_from_secs(&date, secs);
        assert(res == 0);

        write_log1(logger, value, &date);

        if (secs - log2_last_write >= PERIOD_LOG2) {
            f64 avg = get_avg_log1(logger, PERIOD_LOG2, secs);
            if (avg == INFINITY)
                continue;
            write_log2(logger, avg, &date);
            log2_last_write = secs;
        }

        if (secs - log3_last_write >= PERIOD_LOG3) {
            f64 avg = get_avg_log2(logger, PERIOD_LOG3, secs);
            if (avg == INFINITY)
                continue;
            write_log3(logger, avg, &date);
            log3_last_write = secs;
        }
    }

    if (deinit_logger(logger) == -1) 
        perror("Failed to deinit logger");

    if (fclose(dev_file) == -1)
        perror("Failed to close device");

    printf("Log writing finished\n");

    return 0;
}

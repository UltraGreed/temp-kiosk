#include "temp_logger.h"

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
#include "my_types.h"
#include "utils.h"

static bool is_working = true;
void sigint_handler(int sig)
{
    (void)sig;
    is_working = false;
}

void skip_till_value(FILE *dev_file)
{
    while (fgetc(dev_file) != DELIM) {};
}

void skip_old_values(FILE *dev_file)
{
    time_t t = time(NULL);
    while (t == time(NULL))
        fgetc(dev_file);
}

/// Read value from device.
/// Return 0 on success, -1 on error;
int read_value(FILE *dev, f64 *value)
{
    // *value = 15.0 + (f64) rand() / RAND_MAX * 3;
    // sleep(1);
    // return 0;
    char temp_str[MSG_LEN + 1];
    void *res = fgets(temp_str, MSG_LEN + 1, dev);
    if (res == NULL) {
        perror("Failed to read from device");
        return -1;
    }

    if (temp_str[MSG_LEN - 1] != DELIM) {
        perror("Damaged message length from device");
        skip_till_value(dev); // Skip all bytes till delim (including)
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

int write_log_with_avg(Log *log_out, Log *log_in, f64 avg_period, f64 keep_period, DateTime *date)
{
    f64 avg = get_avg_log(log_in, avg_period, date);
    if (avg == INFINITY) {
        fprintf(stderr, "Failed to get average from log!\n");
        return -1;
    }
    int res = write_log(log_out, avg, date, keep_period);
    if (res == -1) {
        fprintf(stderr, "Failed to write to log!\n");
        return -1;
    }
    return 0;
}

int delete_old_logs_entries(Log **logs, DateTime *date)
{
    int res1 = delete_old_entries(logs[0], date, MAX_KEEP_LOG1);
    int res2 = delete_old_entries(logs[1], date, MAX_KEEP_LOG2);
    int res3 = delete_old_entries(logs[2], date, MAX_KEEP_LOG3);
    return (res1 == 0) && (res2 == 0) && (res3 == 0);
}

// TODO: prefix each stderr message with either FAIL or WARN, depending on severity
int main(int argc, char *argv[])
{
    int res;
    if (argc != 3) {
        fprintf(stderr, "Usage: temp_logger DEVICE LOG_PATH\n");
        exit(2);
    }
    signal(SIGINT, sigint_handler);

    const char *dev_name = argv[1];
    FILE *dev_file = xfopen(dev_name, "r");

    Log *logs[3];
    for (int i = 0; i < 3; i++)
        logs[i] = init_log(argv[2], LOG_ARGS[i]);

    DateTime date;
    get_datetime_now(&date);
    delete_old_logs_entries(logs, &date);
    
    fprintf(stderr, "Successfully initialized logs.\n");

    skip_old_values(dev_file);
    skip_till_value(dev_file);

    f64 log2_last_write = get_secs();
    f64 log3_last_write = log2_last_write;

    while (is_working) {
        f64 value;
        read_value(dev_file, &value);

        f64 secs = get_secs();
        DateTime date;
        get_datetime_from_secs(&date, secs);

        res = write_log(logs[0], value, &date, MAX_KEEP_LOG1);
        if (res == -1)
            fprintf(stderr, "Failed to write log 1! Skipping...");

        if (secs - log2_last_write >= PERIOD_LOG2) {
            int res = write_log_with_avg(logs[1], logs[0], PERIOD_LOG2, MAX_KEEP_LOG2, &date);
            if (res == -1)
                fprintf(stderr, "Failed to write log 2! Skipping...");
            else 
                log2_last_write = secs;
        }

        if (secs - log3_last_write >= PERIOD_LOG3) {
            int res = write_log_with_avg(logs[2], logs[1], PERIOD_LOG3, MAX_KEEP_LOG3, &date);
            if (res == -1)
                fprintf(stderr, "Failed to write log 3! Skipping...");
            else
                log3_last_write = secs;
        }
    }

    for (int i = 0; i < 3; i++)
        if (deinit_log(logs[i]))
            fprintf(stderr, "Failed to deinit log %d\n", i);

    if (fclose(dev_file) == -1)
        perror("Failed to close device");

    printf("Log writing finished\n");

    return 0;
}

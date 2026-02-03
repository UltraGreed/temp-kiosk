#pragma once

#include "utils/my_types.h"
#include "cross_time.h"


struct Logger;
typedef struct Logger Logger;


/// Initialize Logger structure. Should be paired with deinit_logger.
Logger *create_logger(char *params[], f64 secs);

/// Deinitialize Logger structure.
int deinit_logger(Logger *logger);

/// Write new date-value to logs.
int write_log1(Logger *logger, f64 value, DateTime *date);
int write_log2(Logger *logger, f64 value, DateTime *date);
int write_log3(Logger *logger, f64 value, DateTime *date);

// Return average within given period from given date in i-th log.
f64 get_avg_log1(Logger *logger, f64 period, f64 secs);
f64 get_avg_log2(Logger *logger, f64 period, f64 secs);


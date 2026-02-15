#pragma once

#include "my_types.h"
#include "cross_time.h"


struct Log;
typedef struct Log Log;

typedef struct {
    DateTime date;
    f64 temp;
} TempEntry;

typedef struct {
    TempEntry *items;
    usize size;
} TempArray;


/// Initialize Log structure. 
/// The caller is responsible for freeing memory with deinit_log.
/// Exit with code 1 on failure.
Log *init_log(const char db_path[], const char table_name[]);

/// Deinitialize Log structure.
int deinit_log(Log *log);

/// Write new date-value to log, deleting old ones.
/// It is not guaranteed that all the old values will be removed on first call.
int write_log(Log *log, f64 value, DateTime *date, usize max_period);

// Return average within given period from given date in log.
f64 get_avg_log(Log *log, f64 period, DateTime *date);

/// Delete all invalid or old log entries.
/// Return 0 on success, -1 on error.
int delete_old_entries(Log *log, DateTime *date, usize max_period);

/// Get an array of all entries within the provided date range.
/// Caller is responsible for memory freeing.
/// If invalid entry is encountered it is replaced with (TempEntry){0}.
/// Return pointer to allocated TempArray or NULL on error.
TempArray *get_array_entries(Log *log, const DateTime *date_start, const DateTime *date_end);

#pragma once

#include <time.h>

#include "my_types.h"

typedef struct {
    f64 secs;
    u16 year;
    u8 month;
    u8 day;
    u8 hours;
    u8 mins;
} DateTime;

// Kolhozno a bit but soidet
#define FIRST_DATE (DateTime){.year = 1, .day = 1, .month = 1, .hours = 0, .mins = 0, .secs = 0}
#define LAST_DATE (DateTime){.year = 9999, .day = 31, .month = 12, .hours = 23, .mins = 59, .secs = 59.999}

/// Get elapsed time since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
/// Exit on fail.
f64 get_secs(void);

/// Fill provided datetime object from time.h struct tm.
/// Can't fail.
void get_datetime_from_tm(DateTime *date, struct tm *tm);

/// Fill provided datetime object from seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).
/// Exit on fail.
void get_datetime_from_secs(DateTime *date, f64 secs);

/// Fill provided datetime object
/// Exit on fail.
int get_datetime_now(DateTime *date);

/// Convert date to the time in seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).
/// Return time in seconds on success, or INFINITY on error.
f64 to_secs(DateTime *date);

/// Scan date from string in "YYYY-MM-DD hh:mm:ss.sss" format.
/// Return 0 on success, -1 on error.
int scan_date(const char *s, DateTime *date);

/// Scan date from string in provided format.
/// Return 0 on success, -1 on error.
int scan_date_fmt(const char *s, DateTime *date, const char *date_fmt);

/// Print date to string in "YYYY-MM-DD hh:mm:ss.sss" format.
/// Can't fail.
void print_date(char *s, const DateTime *date);

#ifdef CROSS_TIME_IMPL
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void get_datetime_from_tm(DateTime *date, struct tm *tm)
{
    *date = (DateTime){
        .year = (u16)tm->tm_year + 1900,
        .month = (u8)tm->tm_mon + 1,
        .day = (u8)tm->tm_mday,
        .hours = (u8)tm->tm_hour,
        .mins = (u8)tm->tm_min,
        .secs = (f64)tm->tm_sec,
    };
}

void get_datetime_from_secs(DateTime *date, f64 secs)
{
    i64 t = (i64)secs;
    struct tm *tp = localtime(&t);
    if (tp == NULL) {
        perror("Failed to get system localtime! Exiting...");
        exit(1);
    }

    get_datetime_from_tm(date, tp);
    date->secs += modf(secs, &(double){0});
}

int get_datetime_now(DateTime *date)
{
    f64 secs = get_secs();
    get_datetime_from_secs(date, secs);
    return 0;
}

f64 to_secs(DateTime *date)
{
    struct tm tp = {
        .tm_year = (int)date->year - 1900,
        .tm_mon = (int)date->month - 1,
        .tm_mday = (int)date->day,
        .tm_hour = (int)date->hours,
        .tm_min = (int)date->mins,
        .tm_sec = (int)date->secs,
        .tm_isdst = -1,
    };
    i64 secs = mktime(&tp);
    if (secs == -1)
        return INFINITY;

    return (f64)secs + modf(date->secs, &(double){0});
}

int scan_date(const char *s, DateTime *date)
{
    return scan_date_fmt(s, date, "%d-%d-%d %d:%d:%lf");
}

int scan_date_fmt(const char *s, DateTime *date, const char *fmt)
{
    assert(s != NULL);

    i32 year, month, day, hours, minutes;
    f64 seconds;

    int res = sscanf(s, fmt, &year, &month, &day, &hours, &minutes, &seconds);
    if (res != 6)
        return -1;

    *date = (DateTime){
        .year = (u16)year,
        .month = (u8)month,
        .day = (u8)day,
        .hours = (u8)hours,
        .mins = (u8)minutes,
        .secs = (f64)seconds,
    };

    return 0;
}

void print_date(char *s, const DateTime *date)
{
    int res = sprintf(s, "%04u-%02u-%02u %02u:%02u:%06.3lf", date->year, date->month, date->day, date->hours,
                      date->mins, date->secs);
    assert(res == 23);
}
#endif

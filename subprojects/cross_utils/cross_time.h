#pragma once

#include "my_types.h"
#include <time.h>

typedef struct {
    u16 year;
    u8 month;
    u8 day;
    u8 hours;
    u8 mins;
    f64 secs;
} DateTime;


/// Get elapsed time since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
/// Return time in seconds on success, or INFINITY on error.
f64 get_secs(void);

/// Fill provided datetime object from time.h struct tm.
void get_datetime_from_tm(DateTime *date, struct tm *tm);

/// Fill provided datetime object from seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).
/// Return 0 on success, -1 otherwise
int get_datetime_from_secs(DateTime *date, f64 secs);

/// Fill provided datetime object
/// Return 0 on success, -1 otherwise
int get_datetime_now(DateTime *date);

/// Convert date to the time in seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC).
/// Return time in seconds on success, or INFINITY on error.
f64 to_secs(DateTime *date);

/// Scan date from string in "YYYY-MM-DD hh:mm:ss.sss" format.
/// Return 0 on success, -1 on error.
int scan_date(char *s, DateTime *date);

/// Print date to string in "YYYY-MM-DD hh:mm:ss.sss" format.
/// Return 0 on success, -1 on error.
int print_date(char *s, DateTime *date);

#ifdef CROSS_TIME_IMPL
#include <stdio.h>
#include <math.h>

void get_datetime_from_tm(DateTime *date, struct tm *tm) 
{
    *date = (DateTime) {
        .year = tm->tm_year + 1900,
        .month = tm->tm_mon + 1,
        .day = tm->tm_mday,
        .hours = tm->tm_hour,
        .mins = tm->tm_min,
        .secs = tm->tm_sec,
    };
}

int get_datetime_from_secs(DateTime *date, f64 secs)
{
    i64 t = secs;
    struct tm *tp = localtime(&t);
    if (tp == NULL)
        return -1;

    get_datetime_from_tm(date, tp);
    date->secs += modf(secs, &(double){0});

    return 0;
}

int get_datetime_now(DateTime *date)
{
    f64 secs = get_secs();
    if (secs == INFINITY)
        return -1;

    int res = get_datetime_from_secs(date, secs);
    if (res == -1)
        return -1;

    return 0;
}

f64 to_secs(DateTime *date)
{
    struct tm tp = {
        .tm_year = date->year - 1900,
        .tm_mon = date->month - 1,
        .tm_mday = date->day,
        .tm_hour = date->hours,
        .tm_min = date->mins,
        .tm_sec = date->secs,
        .tm_isdst = -1,
    };
    i64 secs = mktime(&tp);
    if (secs == -1) 
        return INFINITY;

    return secs + modf(date->secs, &(double){0});
}

int scan_date(char *s, DateTime *date)
{
    i32 year, month, day, hours, minutes;
    f64 seconds;

    int res = sscanf(s, "%d-%d-%d %d:%d:%lf", &year, &month, &day, &hours, &minutes, &seconds);
    if (res != 6) {
        perror("Scan_date fail");
        return -1;
    }

    *date = (DateTime){
        .year = year,
        .month = month,
        .day = day,
        .hours = hours,
        .mins = minutes,
        .secs = seconds,
    };

    return 0;
}

int print_date(char *s, DateTime *date)
{
    int res = sprintf(s, "%04d-%02d-%02d %02d:%02d:%06.3lf", date->year, date->month, date->day, date->hours,
                      date->mins, date->secs);
    if (res != 23)
        return -1;
    return 0;
}
#endif

#include "cross_time.h"

#include <minwindef.h>
#include <stdio.h>
#include <sysinfoapi.h>
#include <time.h>


int get_my_datetime(TIME_S *mt) {
    i64 t = time(NULL);

    struct tm *tm = localtime(&t);
    if (tm == NULL) {
        perror("Unable to get localtime");
        return -1;
    }

    FILETIME ft;
    usize t_ns = 0;
    GetSystemTimeAsFileTime(&ft);
    t_ns |= ft.dwHighDateTime;
    t_ns <<= 32;
    t_ns |= ft.dwLowDateTime;

    *mt = (TIME_S) {
        .month = tm->tm_mon + 1,
        .day = tm->tm_mday,
        .year = tm->tm_year + 1900,
        .hours = tm->tm_hour,
        .mins = tm->tm_min,
        .secs = tm->tm_sec + t_ns % 10000000 * 1e-7,
    };
    return 0;
}


f64 get_secs(void) {
    FILETIME ft;
    usize t_ns = 0;
    GetSystemTimeAsFileTime(&ft);
    t_ns |= ft.dwHighDateTime;
    t_ns <<= 32;
    t_ns |= ft.dwLowDateTime;

    return t_ns * 1e-7;
}

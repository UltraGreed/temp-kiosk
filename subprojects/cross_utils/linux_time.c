#include <cross_time.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

int get_my_datetime(TIME_S *mt) {
    i64 t = time(NULL);

    struct tm *tm = localtime(&t);
    if (tm == NULL) {
        perror("Unable to get localtime");
        return -1;
    }

    struct timespec tp;
    int clock_res = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (clock_res == -1) {
        perror("Unable to get clock time");
        return -1;
    }

    *mt = (TIME_S) {
        .month = tm->tm_mon + 1,
        .day = tm->tm_mday,
        .year = tm->tm_year + 1900,
        .hours = tm->tm_hour,
        .mins = tm->tm_min,
        .secs = tm->tm_sec + (tp.tv_nsec % 1000000000) * 1e-9,
    };
    return 0;
}

f64 get_secs(void) {
    struct timespec tp;
    int clock_res = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(clock_res == 0);
    return tp.tv_sec + tp.tv_nsec * 1e-9;
}

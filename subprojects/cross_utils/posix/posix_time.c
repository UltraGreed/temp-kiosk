#include <stdio.h>
#define CROSS_TIME_IMPL
#include "cross_time.h"
#undef CROSS_TIME_IMPL

#include <assert.h>
#include <sys/time.h>
#include <bits/types/struct_timeval.h>
#include <stdlib.h>

#include "my_types.h"

f64 get_secs(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        perror("Unable to access system clock! Exiting...");
        exit(1);
    }

    return (double) tv.tv_sec + (double) tv.tv_usec * 1e-6;
}


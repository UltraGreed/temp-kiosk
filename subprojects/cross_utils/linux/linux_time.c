#define CROSS_TIME_IMPL
#include "cross_time.h"
#undef CROSS_TIME_IMPL

#include <assert.h>
#include <sys/time.h>
#include <math.h>

f64 get_secs(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) 
        return INFINITY;

    return tv.tv_sec + tv.tv_usec * 1e-6;
}


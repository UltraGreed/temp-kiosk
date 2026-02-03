#define CROSS_TIME_IMPL
#include "cross_time.h"
#undef CROSS_TIME_IMPL

#include <windows.h>


// #define WIN_MORON1 10000
//                    13414581552139
// #define WIN_MORON2 116444736000000000LL
#define WIN_MORON 116444736000000000LL

f64 get_secs(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    usize ticks = 0;
    ticks |= ft.dwHighDateTime;
    ticks <<= 32;
    ticks |= ft.dwLowDateTime;
    
    // i64 millis = ticks / WIN_MORON1;
    // printf("Current millis: %lld\n", millis);
    f64 secs = (ticks - WIN_MORON) * 1e-7;

    printf("Current secs: %f\n", secs);
    return secs;
} 

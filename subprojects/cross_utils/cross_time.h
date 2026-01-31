#pragma once

#include "my_types.h"

typedef struct {
    u8 month;
    u8 day;
    u16 year;
    u8 hours;
    u8 mins;
    double secs;
} cross_time;

/// Fill provided datetime object
/// Return 0 on success, -1 otherwise
int get_my_datetime(cross_time *mt);


/// Return time elapsed since program start in seconds
f64 get_secs(void);



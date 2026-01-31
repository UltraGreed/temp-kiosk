#pragma once

#include "my_types.h"

typedef struct {
    u8 month;
    u8 day;
    u16 year;
    u8 hours;
    u8 mins;
    double secs;
} TIME_S;

/// Fill provided datetime object
/// Return 0 on success, -1 otherwise
int get_my_datetime(TIME_S *mt);


/// Return time elapsed since somewhere in history
f64 get_secs(void);



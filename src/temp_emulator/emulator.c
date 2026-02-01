#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "my_types.h"
#include "utils/files_utils.h"

#define TEMP_MIN 15.0
#define TEMP_MAX 25.0
#define MAX_CHANGE 0.5
#define MAX_BIAS 0.1
#define MSG_INTER 1.0
#define MSG_LEN 8

f64 init_temp(void) {
    return TEMP_MIN + (f64)rand() / RAND_MAX * (TEMP_MAX - TEMP_MIN);
}

f64 next_temp(f64 temp) {
    // Random offset in range [-1, 1]
    f64 direction = (f64)rand() / RAND_MAX * 2 - 1;

    // Biased to the middle
    f64 x = (f64)(temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
    x = fmax(fmin(x, 1), 0);
    f64 bias = cos(x * M_PI);

    return temp + direction * MAX_CHANGE + bias * MAX_BIAS;
}

static bool is_working = true;
void sigint_handler(int sig) {
    (void) sig;
    is_working = false;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: emulator DEVICE\n");
        exit(2);
    }

    signal(SIGINT, sigint_handler);

    const char *dev_name = argv[1];
    FILE *dev_file = fopen(dev_name, "w");
    if (dev_file == NULL) {
        fprintf(stderr, "Failed to open device %s: %s (%d)\n", dev_name, strerror(errno), errno);
        exit(1);
    }

    const int seed = 34;
    srand(seed);

    f64 temp = init_temp();
    while (is_working) {
        temp = next_temp(temp);
        printf("TEMP: %f\n", temp);

        // Format temp to 8 symbols and preserve space sign
        i32 fwidth = MSG_LEN - (i32)flen(temp) - 2;
        fprintf(dev_file, "%.*f\n", fwidth > 0 ? fwidth : 0, temp);
        fflush(dev_file);

        sleep(MSG_INTER);
    }

    fclose(dev_file);
    return 0;
}

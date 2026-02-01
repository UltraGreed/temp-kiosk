#include "files_utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "str_utils.h"
#include "my_types.h"

usize flen(f64 x) {
    usize len = 0;
    do {
        len++;
        x /= 10;
    } while (x >= 1);
    return len;
}

FILE *fopen_or_exit(const char *fname, const char *mode) {
    FILE *file = fopen(fname, mode);
    if (file == NULL) {
        fprintf(stderr, "Unable to open %s: %s\n", (fname), strerror(errno));
        exit(1);
    }
    if (streql(mode, "a+")) // this punk is not cross-platform for some reason
        rewind(file);
    return file;
}

int fsize(FILE *file) {
    int fpos = ftell(file);
    if (fpos == -1)
        return -1;
    fseek(file, 0, SEEK_END);

    int size = ftell(file);

    fseek(file, fpos, SEEK_SET);

    return size;
}

int fatend(FILE *file) {
    int fpos = ftell(file);
    if (fpos == -1)
        return -1;

    return fpos == fsize(file);
}


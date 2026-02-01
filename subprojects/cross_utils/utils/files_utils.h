#include <stdio.h>
#include "my_types.h"

/// Return n digits before dot in x
usize flen(f64 x);

/// Open file or exit if failed
FILE *fopen_or_exit(const char *fname, const char *mode);

/// Return -1 if error, file size otherwise
int fsize(FILE *file);

/// Return -1 if error, 1 if at the end of file, 0 otherwise
int fatend(FILE *file);

#include <stdio.h>
#include <stdbool.h>

#include "my_types.h"

/// FILE UTILS
/// Count digits in integer part of x
usize flen(f64 x);

/// Open file or exit if failed
FILE *xfopen(const char *fname, const char *mode);

/// Return -1 if error, file size otherwise
int fsize(FILE *file);

/// Return -1 if error, 1 if at the end of file, 0 otherwise
int fatend(FILE *file);

/// Return newly allocated memory with joined paths, exit on fail
void *join_paths_xmalloc(const char *path1, const char *path2);

/// STRING UTILS
/// Check whether str starts with prefix
bool starts_with(const char *str, const char *prefix);

/// Check whether s1 equals s2
bool streql(const char *s1, const char *s2);

/// Return newly allocated memory with concatenated strings, exit on fail
void *strcat_xmalloc(const char *str1, const char *str2);

/// MEMORY UTILS
/// Call malloc and exit with perror on error;
void *xmalloc(usize size);

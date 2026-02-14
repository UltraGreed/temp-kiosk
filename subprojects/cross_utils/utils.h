#include <stdio.h>
#include <stdbool.h>

#include "my_types.h"

/// FILE UTILS
/// Count digits in integer part of x.
usize flen(f64 x);

/// Open file or exit if failed.
FILE *xfopen(const char *fname, const char *mode);

/// Return -1 if error, file size otherwise.
i64 fsize(FILE *file);

/// Return -1 if error, 1 if at the end of file, 0 otherwise.
int fatend(FILE *file);

/// Truncate file to provided size.
/// Return -1 if error, 0 otherwise.
int ftrunc(FILE *file, usize n);

/// Return newly allocated memory with joined paths, exit on fail.
void *join_paths_xmalloc(const char *path1, const char *path2);

/// STRING UTILS
/// Check whether str starts with prefix.
bool starts_with(const char *str, const char *prefix);

/// Check whether s1 equals s2.
bool streql(const char *s1, const char *s2);

/// Return newly allocated memory with concatenated strings, exit on fail.
void *strcat_xmalloc(const char *str1, const char *str2);

/// Return true if provided null-terminated string is valid ASCII sequence.
bool is_valid_ascii(const u8 s[]);

/// MEMORY UTILS
/// Call malloc and exit with perror on error.
void *xmalloc(usize size);

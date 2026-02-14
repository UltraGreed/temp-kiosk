#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "my_types.h"

/// FILE UTILS
usize flen(f64 x)
{
    usize len = 0;
    do {
        len++;
        x /= 10;
    } while (x >= 1);
    return len;
}

FILE *xfopen(const char *fname, const char *mode)
{
    FILE *file = fopen(fname, mode);
    if (file == NULL) {
        fprintf(stderr, "Failed to open file %s: %s(%d)\n", (fname), strerror(errno), errno);
        assert(file != NULL); // Exit with assert if in debug mode for more helpful message.
        exit(1);
    }
    if (streql(mode, "a+")) // this punk's cursor position is not cross-platform for some reason
        rewind(file);
    return file;
}

i64 fsize(FILE *file)
{
    i64 fpos = ftello(file);
    if (fpos == -1)
        return -1;

    i64 res = fseeko(file, 0, SEEK_END);
    if (res == -1)
        return -1;

    i64 size = ftello(file);
    if (size == -1)
        return -1;

    res = fseeko(file, fpos, SEEK_SET);
    if (res == -1)
        return -1;

    return size;
}

int fatend(FILE *file)
{
    i64 fpos = ftello(file);
    if (fpos == -1)
        return -1;

    return fpos == fsize(file);
}

#ifdef WIN32
int ftrunc(FILE *file, usize n)
{
    return _chsize(fileno(file), n);
}
#else
int ftrunc(FILE *file, usize n)
{
    return ftruncate(fileno(file), n);
}

#endif


void *join_paths_xmalloc(const char *path1, const char *path2)
{
    usize len = strlen(path1) + strlen(path2);
    char *s = xmalloc(len + 2);

    usize res = snprintf(s, len + 2, "%s/%s", path1, path2);
    assert(res == len + 1);

    return s;
}


/// STRING UTILS
bool starts_with(const char *str, const char *prefix)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool streql(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

void *strcat_xmalloc(const char *str1, const char *str2)
{
    usize len = strlen(str1) + strlen(str2);
    char *s = xmalloc(len + 1);

    usize res = snprintf(s, len + 1, "%s%s", str1, str2);
    if (res != len)
        assert(res == len);

    return s;
}

bool is_valid_ascii(const u8 s[])
{
    for (const u8 *c = s; c != NULL; c++)
        if (*c > 127) 
            return false;
    return true;
}

/// MEMORY UTILS
void *xmalloc(usize size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Unable to allocate memory of %zu bytes: %s(%d)", size, strerror(errno), errno);
        assert(ptr != NULL); // Exit with assert if in debug mode for more helpful message.
        exit(1);
    }
    return ptr;
}

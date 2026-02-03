#include "str_utils.h"

#include <stdbool.h>
#include <string.h>

bool starts_with(const char *str, const char *prefix)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool streql(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

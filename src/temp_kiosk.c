#include <stdio.h>

int main(void) {
#ifdef _WIN32
    printf("All your codebase are belong to mini mild!\n");
#else
    printf("All your codebase are belong to us!\n");
#endif
    return 0;
}

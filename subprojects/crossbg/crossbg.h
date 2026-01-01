#ifndef crossbg_h
#define crossbg_h

#include "my_types.h"

#ifdef __linux__
typedef usize proc_t;
#else
#include <windows.h>
typedef HANDLE proc_t;
#endif

/// Start executing *command* with provided args in background, set pid.
/// Return errno if fork fails, 0 otherwise.
/// Return 0 if successful, otherwise errno on Linux, GetLastError on Windows.
usize bg_start(proc_t *proc, const char *command, char *const argv[]);

/// Enum for bg_wait result
typedef enum {
    BG_WEXITED,  // Process exited or was terminated
    BG_WTIMEOUT, // Timeout exceeded, process still runs
    BG_WFAIL,    // Wait failed
} BG_WRES;

/// Wait for given pid to finish, write exit status (only meaningful on normal exit).
/// Timeout in seconds can optionally be provided. Set to 0 for no limit.
/// If timeout exceeded, program execution continues, process is not killed.
/// Return enum representing waiting result.
BG_WRES bg_wait(proc_t pid, usize timeout, i32 *status);

/// Send SIGINT to process with given pid.
/// Return 0 if successful, otherwise errno on Linux, GetLastError on Windows.
usize bg_kill(proc_t pid);

#endif

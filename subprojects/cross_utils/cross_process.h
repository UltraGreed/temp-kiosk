#pragma once

#include "my_types.h"
#include "utils.h"

#ifdef __linux__
#include <time.h>
typedef pid_t Process;
#else
#include <windows.h>
typedef HANDLE Process;
#endif

/// Start executing *command* with provided args in background, set pid.
/// Return errno if fork fails, 0 otherwise.
/// Return 0 if successful, otherwise errno on Linux, GetLastError on Windows.
usize start_process(Process *proc, const char *command, char *const argv[]);

/// Enum for bg_wait result
typedef enum {
    BG_WEXITED,  // Process exited or was terminated
    BG_WTIMEOUT, // Timeout exceeded, process still runs
    BG_WFAIL,    // Wait failed
} ProcessWaitResult;

/// Wait for given pid to finish, write exit status (only meaningful on normal exit).
/// Timeout in seconds can optionally be provided. Set to 0 for no limit.
/// If timeout exceeded, program execution continues, process is not killed.
/// Return enum representing waiting result.
ProcessWaitResult wait_process(Process pid, f64 timeout, i32 *status);

/// Send SIGINT to process with given pid.
/// Return 0 if successful, otherwise errno on Linux, GetLastError on Windows.
usize kill_process(Process pid);

bool is_process_running(Process proc);

#ifdef CROSS_PROCESS_IMPL
bool is_process_running(Process proc)
{
    ProcessWaitResult w_res = wait_process(proc, 1e-5, NULL);
    return w_res == BG_WTIMEOUT;
}
#endif

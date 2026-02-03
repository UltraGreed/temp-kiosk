#define CROSS_PROCESS_IMPL
#include "cross_process.h"
#undef CROSS_PROCESS_IMPL

#include <errhandlingapi.h>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <windows.h>
#include <winerror.h>
#include <winnt.h>

#include "utils/my_types.h"

static char *join_strings(char *const strings[])
{
    usize total_len = 0;
    for (char *const *arg = strings; *arg != NULL; arg++)
        total_len += strlen(*arg) + 1;
    total_len--;

    char *new_str = malloc(sizeof(char) * (total_len + 1));
    new_str[total_len] = '\0';

    char *cur_sym = new_str;
    for (char *const *arg = strings; *arg != NULL; arg++) {
        usize len = strlen(*arg);
        memcpy(cur_sym, *arg, len);
        if (cur_sym + len != new_str + total_len) {
            cur_sym[len] = ' ';
            cur_sym += len + 1;
        }
    }

    return new_str;
}

usize start_process(Process *proc, const char *command, char *const argv[])
{
    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    char *cmd_with_args = join_strings((char *const[]){command, join_strings(argv + 1)});
    if (!CreateProcess(NULL, cmd_with_args, NULL, NULL, false, 0, NULL, NULL, &si, &pi))
        return GetLastError();
    free(cmd_with_args);

    CloseHandle(pi.hThread);
    *proc = pi.hProcess;
    return 0;
}

ProcessWaitResult wait_process(Process proc, f64 timeout, i32 *status)
{
    ProcessWaitResult res;
    if (timeout == 0)
        timeout = INFINITE;

    u32 wres = WaitForSingleObject(proc, timeout * 1000);

    switch (wres) {
    case WAIT_TIMEOUT: res = BG_WTIMEOUT; goto end;
    case WAIT_ABANDONED:
    case WAIT_FAILED: res = BG_WFAIL; goto close;
    }

    DWORD exit_code;
    if (!GetExitCodeProcess(proc, &exit_code)) {
        res = BG_WFAIL;
        goto close;
    }

    if (status != NULL)
        *status = exit_code;
    res = BG_WEXITED;

close:
    CloseHandle(proc);
end:
    return res;
}

usize kill_process(Process proc)
{
    if (!TerminateProcess(proc, STATUS_CONTROL_C_EXIT))
        return GetLastError();
    return 0;
}

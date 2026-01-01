#include "crossbg.h"
#include <stdarg.h>

#ifdef __linux__
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <handleapi.h>
#include <synchapi.h>
#include <winerror.h>
#include <winnt.h>
#include <errhandlingapi.h>
#include <processthreadsapi.h>
#include <windows.h>
#endif

#ifdef __linux__
usize bg_start(proc_t *proc, const char *command, char *const argv[]) {
    i32 pipefd[2];
    pipe(pipefd);

    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    i32 forkres = fork();
    if (forkres == -1) {
        return errno;
    } else if (forkres == 0) { // child
        close(pipefd[0]);
        if (execvp(command, argv) == -1) {
            i32 err = errno;
            write(pipefd[1], &err, sizeof(err));
            _exit(127);
        }
    }

    // parent
    close(pipefd[1]);

    i32 err;
    // FIXME: parent will hang if child hangs
    usize n = read(pipefd[0], &err, sizeof(err));
    close(pipefd[0]);

    if (n > 0) { // Child start failed with errno
        waitpid(forkres, NULL, 0);
        return err;
    }

    *proc = forkres;
    return 0;
}

BG_WRES bg_wait(proc_t proc, usize timeout, i32 *status) {
    i32 wstatus;

    usize tstart = time(NULL);
    while (time(NULL) - tstart < timeout) {
        i32 wres = waitpid(proc, &wstatus, WNOHANG);
        if (wres == -1)
            return BG_WFAIL;
        if (wres != 0) {
            if (WIFEXITED(wstatus))
                *status = WEXITSTATUS(wstatus);
            return BG_WEXITED;
        }
        usleep(100000);
    }
    return BG_WTIMEOUT;
}

usize bg_kill(proc_t proc) {
    if (kill(proc, SIGINT) == 0)
        return 0;
    return errno;
}
#else
static char *join_strings(char *const strings[]) {
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

usize bg_start(proc_t *proc, const char *command, char *const argv[]) {
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

BG_WRES bg_wait(proc_t proc, usize timeout, i32 *status) {
    BG_WRES res;
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

    *status = exit_code;
    res = BG_WEXITED;

close:
    CloseHandle(proc);
end:
    return res;
}

usize bg_kill(proc_t proc) {
    if (!TerminateProcess(proc, STATUS_CONTROL_C_EXIT))
        return GetLastError();
    return 0;
}
#endif

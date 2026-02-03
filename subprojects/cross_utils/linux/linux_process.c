#define CROSS_PROCESS_IMPL
#include "cross_process.h"
#undef CROSS_PROCESS_IMPL

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cross_time.h"
#include "utils/my_types.h"

usize start_process(Process *proc, const char *command, char *const argv[])
{
    i32 pipefd[2];
    pipe(pipefd);

    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    pid_t forkres = fork();
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

    int err_rec;
    // FIXME: parent will hang if child hangs
    usize n = read(pipefd[0], &err_rec, sizeof(err_rec));
    close(pipefd[0]);

    if (n > 0) { // Child start failed with errno
        waitpid(forkres, NULL, 0);
        return err_rec;
    }

    *proc = forkres;
    return 0;
}

ProcessWaitResult wait_process(Process proc, f64 timeout, i32 *status)
{
    i32 wstatus;

    f64 time_start = get_secs();
    while (!timeout || get_secs() - time_start < timeout) {
        i32 wres = waitpid(proc, &wstatus, WNOHANG);
        if (wres == -1)
            return BG_WFAIL;
        if (wres != 0) {
            if (status != NULL && WIFEXITED(wstatus))
                *status = WEXITSTATUS(wstatus);
            return BG_WEXITED;
        }
        usleep(1000);
    }
    return BG_WTIMEOUT;
}

usize kill_process(Process proc)
{
    if (kill(proc, SIGINT) == 0)
        return 0;
    return errno;
}

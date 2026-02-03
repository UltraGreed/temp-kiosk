#include "cross_process.h"

#include <stdio.h>
#include <unistd.h>

#define TEST_TRUE(x)                                                                                         \
    if (!(x))                                                                                                \
        return 1;

#ifdef __linux__
const char *command = "/usr/bin/sleep";
char *command_name = "sleep";
#define ARGS(x) x
const usize no_process_code = 3;
#else
static const char *command = "ping";
char *command_name = "ping";
#define ARGS(x) "-n", x, "127.0.0.1"
const usize no_process_code = 6;
#endif

bool test_timeout(void)
{
    Process pid;
    char *const argv[] = {command_name, ARGS("10"), NULL};
    usize start_res = start_process(&pid, command, argv);

    i32 status;
    usize wait_res = wait_process(pid, 1, &status);

    usize kill_res = kill_process(pid);

    return start_res == 0 && wait_res == BG_WTIMEOUT && kill_res == 0;
}

bool test_success(void)
{
    Process pid;
    char *const argv[] = {command_name, ARGS("1"), NULL};
    usize start_res = start_process(&pid, command, argv);

    i32 status;
    usize wait_res = wait_process(pid, 5, &status);

    usize kill_res = kill_process(pid);
    printf("%zu\n", kill_res);

    return start_res == 0 && wait_res == BG_WEXITED && kill_res == no_process_code && status == 0;
}

bool test_interrupt(void)
{
    Process pid;
    char *const argv[] = {command_name, ARGS("1"), NULL};
    usize start_res = start_process(&pid, command, argv);
    usize kill_res = kill_process(pid);

    i32 status;
    usize wait_res = wait_process(pid, 1, &status);

    return start_res == 0 && wait_res == BG_WEXITED && kill_res == 0;
}

bool test_late_wait(void)
{
    Process pid;
    char *const argv[] = {command_name, ARGS("1"), NULL};
    usize start_res = start_process(&pid, command, argv);

    sleep(1);

    i32 status;
    usize wait_res = wait_process(pid, 1, &status);

    usize kill_res = kill_process(pid);

    return start_res == 0 && wait_res == BG_WEXITED && status == 0 && kill_res == no_process_code;
}

bool test_not_found_command(void)
{
    Process pid;
    const char *command = "/usr/bin/windows";
    char *const argv[] = {"windows", NULL};
    usize start_res = start_process(&pid, command, argv);

    return start_res == 2;
}

int main(void)
{
    TEST_TRUE(test_timeout());
    TEST_TRUE(test_success());
    TEST_TRUE(test_interrupt());
    TEST_TRUE(test_late_wait());
    TEST_TRUE(test_not_found_command());

    return 0;
}

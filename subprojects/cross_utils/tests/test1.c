#include "cross_process.h"

#include <stdio.h>
#include <unistd.h>

#include "my_types.h"

#define TEST_TRUE(x)                                                                                         \
    while (!(x)) {                                                                                           \
        return 1;                                                                                            \
    }

#ifdef WIN32
static const char *test_command = "ping";
char command_name[] = "ping";
static char arg1[] = "-n";
static char arg2[] = "127.0.0.1";
const usize no_process_code = 6;
#define ARGS(x) arg1, x, arg2
#else
const char *test_command = "/usr/bin/sleep";
char command_name[] = "sleep";
#define ARGS(x) x
const usize no_process_code = 3;
#endif

static bool test_timeout(void)
{
    Process pid;
    char secs[] = "10";
    const char *const argv[] = {command_name, ARGS(secs), NULL};
    usize start_res = start_process(&pid, test_command, argv);

    i32 status;
    usize wait_res = wait_process(pid, 1, &status);

    usize kill_res = kill_process(pid);

    return start_res == 0 && wait_res == BG_WTIMEOUT && kill_res == 0;
}

static bool test_success(void)
{
    Process pid;
    char secs[] = "1";
    const char *const argv[] = {command_name, ARGS(secs), NULL};
    usize start_res = start_process(&pid, test_command, argv);

    i32 status;
    usize wait_res = wait_process(pid, 5, &status);

    usize kill_res = kill_process(pid);
    printf("%zu\n", kill_res);

    return start_res == 0 && wait_res == BG_WEXITED && kill_res == no_process_code && status == 0;
}

static bool test_interrupt(void)
{
    Process pid;
    char secs[] = "1";
    const char *const argv[] = {command_name, ARGS(secs), NULL};
    usize start_res = start_process(&pid, test_command, argv);
    usize kill_res = kill_process(pid);

    i32 status;
    usize wait_res = wait_process(pid, 1, &status);

    return start_res == 0 && wait_res == BG_WEXITED && kill_res == 0;
}

static bool test_late_wait(void)
{
    Process pid;
    char secs[] = "1";
    const char *const argv[] = {command_name, ARGS(secs), NULL};
    usize start_res = start_process(&pid, test_command, argv);

    sleep(1);

    i32 status;
    usize wait_res = wait_process(pid, 1, &status);

    usize kill_res = kill_process(pid);

    return start_res == 0 && wait_res == BG_WEXITED && status == 0 && kill_res == no_process_code;
}

static bool test_not_found_command(void)
{
    Process pid;
    char command[] = "/usr/bin/windows";
    const char *const argv[] = {command, NULL};
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

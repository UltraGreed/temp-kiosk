#include "crossbg.h"

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

bool test_timeout(void) {
    proc_t pid;
    char *const argv[] = {command_name, ARGS("10"), NULL};
    usize start_res = bg_start(&pid, command, argv);

    i32 status;
    usize wait_res = bg_wait(pid, 1, &status);

    usize kill_res = bg_kill(pid);

    return start_res == 0 && wait_res == BG_WTIMEOUT && kill_res == 0;
}

bool test_success(void) {
    proc_t pid;
    char *const argv[] = {command_name, ARGS("1"), NULL};
    usize start_res = bg_start(&pid, command, argv);

    i32 status;
    usize wait_res = bg_wait(pid, 5, &status);

    usize kill_res = bg_kill(pid);
    printf("%zu\n", kill_res);

    return start_res == 0 && wait_res == BG_WEXITED && kill_res == no_process_code && status == 0;
}

bool test_interrupt(void) {
    proc_t pid;
    char *const argv[] = {command_name, ARGS("1"), NULL};
    usize start_res = bg_start(&pid, command, argv);
    usize kill_res = bg_kill(pid);

    i32 status;
    usize wait_res = bg_wait(pid, 1, &status);

    return start_res == 0 && wait_res == BG_WEXITED && kill_res == 0;
}

bool test_late_wait(void) {
    proc_t pid;
    char *const argv[] = {command_name, ARGS("1"), NULL};
    usize start_res = bg_start(&pid, command, argv);

    sleep(1);

    i32 status;
    usize wait_res = bg_wait(pid, 1, &status);

    usize kill_res = bg_kill(pid);

    return start_res == 0 && wait_res == BG_WEXITED && status == 0 && kill_res == no_process_code;
}

bool test_not_found_command(void) {
    proc_t pid;
    const char *command = "/usr/bin/windows";
    char *const argv[] = {"windows", NULL};
    usize start_res = bg_start(&pid, command, argv);

    return start_res == 2;
}

int main(void) {
    TEST_TRUE(test_timeout());
    TEST_TRUE(test_success());
    TEST_TRUE(test_interrupt());
    TEST_TRUE(test_late_wait());
    TEST_TRUE(test_not_found_command());

    return 0;
}

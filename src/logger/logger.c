#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "utils/my_types.h"
#include "cross_process.h"
#include "cross_time.h"
#include "cross_mem.h"
#include "utils/str_utils.h"


static volatile sig_atomic_t is_working = true;
static f32 IDLE_INTER = 0.016;
static f32 CTR_INTER = 0.3;
static f32 LOG_INTER = 1.0;
static f32 COPY_INTER = 3.0;
static f32 COPY2_DELAY = 2.0;

/// Ya znau chto tak ne stoit delat, no ya ustal (
#define WRITE_FILE_OR_FAIL(fname, format, ...)                                                               \
    do {                                                                                                     \
        FILE *__file = fopen((fname), "a");                                                                  \
        if (__file == NULL) {                                                                                \
            fprintf(stderr, "Unable to open file %s: %s\n", (fname), strerror(errno));                       \
            return -1;                                                                                       \
        }                                                                                                    \
        if (fprintf(__file, (format), __VA_ARGS__) == -1) {                                                  \
            fprintf(stderr, "Unable to write to file %s: %s\n", (fname), strerror(errno));                   \
            fclose(__file);                                                                                  \
            return -1;                                                                                       \
        }                                                                                                    \
        if (fflush(__file) == EOF) { /* Even flush can fail you... */                                        \
            fprintf(stderr, "Unable to flush file %s: %s\n", (fname), strerror(errno));                      \
            fclose(__file);                                                                                  \
            return -1;                                                                                       \
        }                                                                                                    \
        if (fclose(__file) != 0) {                                                                           \
            fprintf(stderr, "Unable to close file %s: %s\n", (fname), strerror(errno));                      \
            return -1;                                                                                       \
        }                                                                                                    \
    } while (0);

int write_start_copy(const char *fname, int n_copy) {
    DateTime t;
    int time_res = get_datetime_now(&t);
    if (time_res == -1) {
        perror("Unable to get time");
        return -1;
    }

    WRITE_FILE_OR_FAIL(fname, "Copy %d started with pid %d at: %02d-%02d-%d %02d:%02d:%0.3lf\n",
                       n_copy, getpid(), t.day, t.month, t.year, t.hours, t.mins, t.secs);
    return 0;
}

int write_exit_copy(const char *fname, int n_copy) {
    DateTime t;
    int time_res = get_datetime_now(&t);
    if (time_res == -1) {
        perror("Unable to get time");
        return -1;
    }

    WRITE_FILE_OR_FAIL(fname, "Copy %d exiting at: %02d-%02d-%d %02d:%02d:%0.3lf\n",
                       n_copy, t.day, t.month, t.year, t.hours, t.mins, t.secs);
    return 0;
}

/// Write pid, current date and counter.
/// Return 0 on success, -1 on error.
int write_info(const char *fname, i32 ctr) {
    DateTime t;
    int time_res = get_datetime_now(&t);
    if (time_res == -1) {
        perror("Unable to get time");
        return -1;
    }

    pid_t pid = getpid();
    WRITE_FILE_OR_FAIL(fname, "PID: %d, Date: %02d-%02d-%d %02d:%02d:%0.3lf, Counter: %05d\n",
                       pid, t.day, t.month, t.year, t.hours, t.mins, t.secs, ctr);
    return 0;
}

int write_copies_still_running(const char *fname) {
    DateTime t;
    int time_res = get_datetime_now(&t);
    if (time_res == -1) {
        perror("Unable to get time");
        return -1;
    }

    WRITE_FILE_OR_FAIL(fname, "Copies still running at: %02d-%02d-%d %02d:%02d:%0.3lf\n",
                       t.day, t.month, t.year, t.hours, t.mins, t.secs);
    return 0;
}

void sig_handler(int s) {
    (void)s;
    is_working = false;
}

typedef enum {
    MODE_MAIN,
    MODE_COPY1,
    MODE_COPY2,
} MODE;

int wrong_args(void) {
    fprintf(stderr, "Usage: logger [--mode=main|copy1|copy2] LOG_PATH\n");
    return 2;
}

#define TRY_OR_CLEANUP(expr, err_msg)                                                                        \
    do {                                                                                                     \
        if ((expr) == -1) {                                                                                  \
            perror(err_msg);                                                                                 \
            exit_code = 1;                                                                                   \
            goto cleanup;                                                                                    \
        }                                                                                                    \
    } while (0);


static int exit_code = 0;

int main(int argc, const char *argv[]) {
    bool mmapped = false, shm_opened = false, cli_started = false, sem_loaded = false;

    MODE logger_mode;
    const char *fname;
    if (argc == 2) {
        logger_mode = MODE_MAIN;
        fname = argv[1];
    } else if (argc == 3) {
        if (streql(argv[1], "--mode=main"))
            logger_mode = MODE_MAIN;
        else if (streql(argv[1], "--mode=copy1"))
            logger_mode = MODE_COPY1;
        else if (streql(argv[1], "--mode=copy2"))
            logger_mode = MODE_COPY2;
        else
            return wrong_args();
        fname = argv[2];
    } else {
        return wrong_args();
    }

    // Handle Ctrl-C
    signal(SIGINT, sig_handler);

    // ref_ctr counter
    // [xxxx]  [xxxx]  
    int shm_existed = is_exist_shared_mem(LOGGER_SHM_NAME);
    if (shm_existed == -1) {
        perror("Unable to create shared memory");
        exit_code = 1;
        goto cleanup;
    } else if (!shm_existed && logger_mode != MODE_MAIN) { // --mode=copy1|2 can't initialize shm
        const char *mode_name = logger_mode == MODE_COPY1 ? "copy1" : "copy2";
        fprintf(stderr,
                "Shared memory not found! --mode=%s can only be used when at least one process with --mode=main is already running! Aborting...\n",
                mode_name);
        exit_code = 2;
        goto cleanup;
    }

    SharedMemory shm_fd = open_shared_mem(LOGGER_SHM_NAME, LOGGER_SHM_LEN);
    if (shm_fd == (SharedMemory) -1) {
        perror("Unable to create shared memory");
        exit_code = 1;
        goto cleanup;
    }
    shm_opened = true;

    bool is_origin_inst = !shm_existed;

    // Mapping shm to memory
    void *addr = map_shared_mem(shm_fd, LOGGER_SHM_LEN);
    if (addr == (void *)-1) {
        perror("Unable to map shared memory");
        exit_code = 1;
        goto cleanup;
    }
    mmapped = true;

    i32 *ref_ctr = addr;
    i32 *ctr = (i32 *)addr + 1;
    
    Semaphore sem = open_semaphore(LOGGER_SEM_NAME, 1);
    if (sem == (void *) -1) {
        perror("Unable to create or open semaphore");
        exit_code = 1;
        goto cleanup;
    }
    // Initializing shm and sem
    if (is_origin_inst) { // This is the main logger
        *ref_ctr = 0;
        *ctr = 0;
    }
    sem_loaded = true;

    TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
    *ref_ctr += 1;
    TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");

    Process cli_proc;
    if (logger_mode == MODE_MAIN) {
        // Starting CLI subprocess
        usize bg_start_res = start_process(&cli_proc, LOGGER_CLI_CMD, (char *const[]){LOGGER_CLI_CMD, NULL});
        if (bg_start_res != 0) {
            fprintf(stderr, "Unable to start child process: %lu %s\n", bg_start_res, strerror(bg_start_res));
            goto cleanup;
        }
        cli_started = true;

        f64 log_secs = 0;
        f64 copy_secs = get_secs();
        f64 ctr_secs = get_secs();
        bool copies_started = false;
        Process copy1_proc, copy2_proc;
        while (is_working) { // Main loop
            if (is_origin_inst) {
                if (get_secs() - ctr_secs >= CTR_INTER) {
                    ctr_secs = get_secs();
                    TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
                    *ctr += 1;
                    TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");
                }
                if (get_secs() - log_secs >= LOG_INTER) {
                    log_secs = get_secs();

                    TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
                    TRY_OR_CLEANUP(write_info(fname, *ctr), "Unable to write to log");
                    TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");
                }
                if (get_secs() - copy_secs >= COPY_INTER) {
                    copy_secs = get_secs();
                    if (copies_started && (is_process_running(copy1_proc) || is_process_running(copy2_proc))) {
                        write_copies_still_running(fname);
                    } else {
                        int start_copy1_res = start_process(&copy1_proc, LOGGER_CMD,
                                                       (char *const[]){LOGGER_CMD, "--mode=copy1", fname, NULL});
                        if (start_copy1_res != 0) {
                            perror("Unable to start copy1");
                            goto cleanup;
                        }
                        int start_copy2_res = start_process(&copy2_proc, LOGGER_CMD,
                                                       (char *const[]){LOGGER_CMD, "--mode=copy2", fname, NULL});
                        if (start_copy2_res != 0) {
                            perror("Unable to start copy2");
                            goto cleanup;
                        }
                        copies_started = true;
                    }
                }
            } else { // At least one --mode=main is already running --- just wait
                TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
                if (*ref_ctr == 1) { // If all the other mains die, become new main
                    is_origin_inst = true;
                    log_secs = 0;
                    copy_secs = 0;
                }
                TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");
            }
            sleep(IDLE_INTER);
        }
        printf("Log writing finished.\n");
    } else if (logger_mode == MODE_COPY1) {
        TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
        write_start_copy(fname, 1);
        *ctr += 10;
        write_exit_copy(fname, 1);
        TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");
    } else {
        TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
        write_start_copy(fname, 2);
        *ctr *= 2;
        TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");

        sleep(COPY2_DELAY);

        TRY_OR_CLEANUP(wait_semaphore(sem), "Unable to wait for semaphore");
        *ctr /= 2;
        write_exit_copy(fname, 2);
        TRY_OR_CLEANUP(post_semaphore(sem), "Unable to post semaphore");
    }


cleanup:
    if (cli_started)
        kill_process(cli_proc);
    if (sem_loaded) {
        int sem_wait_res = wait_semaphore(sem);
        assert(sem_wait_res == 0); // If it fails, it fails...
        *ref_ctr -= 1;
        bool last_ref = *ref_ctr == 0;
        int sem_post_res = post_semaphore(sem);
        assert(sem_post_res == 0);

        close_semaphore(sem);
        if (last_ref) {
            printf("PID: %d cleared sem and shm\n", getpid());
            unlink_semaphore(LOGGER_SEM_NAME);
            unlink_shared_mem(LOGGER_SHM_NAME);
        }
    }
    if (mmapped)
        unmap_shared_mem(addr, LOGGER_SHM_LEN);
    if (shm_opened)
        close_shared_mem(shm_fd);

    return exit_code;
}

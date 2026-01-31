#include <asm-generic/errno-base.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>

#include "crossbg.h"
#include "logger.h"
#include "my_time.h"
#include "my_types.h"

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
    my_time t;
    int time_res = get_my_datetime(&t);
    if (time_res == -1) {
        perror("Unable to get time");
        return -1;
    }

    WRITE_FILE_OR_FAIL(fname, "Copy %d starting at: %02d-%02d-%d %02d:%02d:%0.3lf\n",
                       n_copy, t.day, t.month, t.year, t.hours, t.mins, t.secs);
    return 0;
}

int write_exit_copy(const char *fname, int n_copy) {
    my_time t;
    int time_res = get_my_datetime(&t);
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
    my_time t;
    int time_res = get_my_datetime(&t);
    if (time_res == -1) {
        perror("Unable to get time");
        return -1;
    }

    pid_t pid = getpid();
    WRITE_FILE_OR_FAIL(fname, "PID: %d, Date: %02d-%02d-%d %02d:%02d:%0.3lf, Counter: %05d\n", pid, t.day,
                       t.month, t.year, t.hours, t.mins, t.secs, ctr);
    return 0;
}

int write_copies_still_running(const char *fname) {
    my_time t;
    int time_res = get_my_datetime(&t);
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

bool starts_with(const char *str, const char *prefix) {
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool streql(const char *s1, const char *s2) {
    return strcmp(s1, s2) == 0;
}

/// Return -1 on error, 1 if shm exists, 0 otherwise
int shm_exists(const char *shm_name) {
    int shm_fd = shm_open(shm_name, O_CREAT | O_EXCL, 0);
    int res;
    if (shm_fd == -1 && errno == EEXIST)
        res = 1;
    else if (shm_fd == -1)
        res = -1;
    else
        res = 0;

    if (res == 0)
        shm_unlink(shm_name);

    return res;
}

#define TRY_OR_CLEANUP(expr, err_msg)                                                                        \
    do {                                                                                                     \
        if ((expr) == -1) {                                                                                  \
            perror(err_msg);                                                                                 \
            exit_code = 1;                                                                                   \
            goto cleanup;                                                                                    \
        }                                                                                                    \
    } while (0);

bool is_proc_running(proc_t proc) {
    BG_WRES w_res = bg_wait(proc, 1e-5, NULL);
    return w_res == BG_WTIMEOUT;
}

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

    // ref_ctr counter semaphore
    // [xxxx]  [xxxx]   [xxxx]
    const char *shm_name = "/temp_kiosk_logger";
    int shm_existed = shm_exists(shm_name);
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
    int shm_flag = O_RDWR | O_CREAT;
    mode_t shm_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int shm_fd = shm_open(shm_name, shm_flag, shm_mode);

    if (shm_fd == -1) {
        perror("Unable to create shared memory");
        exit_code = 1;
        goto cleanup;
    }
    shm_opened = true;

    bool is_origin_inst = !shm_existed;

    if (is_origin_inst)
        TRY_OR_CLEANUP(ftruncate(shm_fd, LOGGER_SHM_LEN), "Unable to truncate shared memory");

    // Mapping shm to memory
    int prot = PROT_READ | PROT_WRITE;
    void *map_res = mmap(NULL, LOGGER_SHM_LEN, prot, MAP_SHARED, shm_fd, 0);
    if (map_res == (void *)-1) {
        perror("Unable to map shared memory");
        exit_code = 1;
        goto cleanup;
    }
    mmapped = true;

    i32 *ref_ctr = map_res;
    i32 *ctr = (i32 *)map_res + 1;
    sem_t *sem = (sem_t *)((i32 *)map_res + 2);

    // Initializing shm and sem
    if (is_origin_inst) { // This is the main logger
        *ref_ctr = 0;
        TRY_OR_CLEANUP(sem_init(sem, 1, 1), "Unable to initialize semaphore");
        *ctr = 0;
    }
    sem_loaded = true;

    TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
    *ref_ctr += 1;
    TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");

    proc_t cli_proc;
    if (logger_mode == MODE_MAIN) {
        // Starting CLI subprocess
        usize bg_start_res = bg_start(&cli_proc, LOGGER_CLI_CMD, (char *const[]){LOGGER_CLI_CMD, NULL});
        if (bg_start_res != 0) {
            fprintf(stderr, "Unable to start child process: %lu %s\n", bg_start_res, strerror(bg_start_res));
            goto cleanup;
        }
        cli_started = true;

        f64 log_secs = 0;
        f64 copy_secs = get_secs();
        f64 ctr_secs = get_secs();
        bool copies_started = false;
        proc_t copy1_proc, copy2_proc;
        while (is_working) { // Main loop
            if (is_origin_inst) {
                if (get_secs() - ctr_secs >= CTR_INTER) {
                    ctr_secs = get_secs();
                    TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
                    *ctr += 1;
                    TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");
                }
                if (get_secs() - log_secs >= LOG_INTER) {
                    log_secs = get_secs();

                    TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
                    TRY_OR_CLEANUP(write_info(fname, *ctr), "Unable to write to log");
                    TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");
                }
                if (get_secs() - copy_secs >= COPY_INTER) {
                    copy_secs = get_secs();
                    if (copies_started && (is_proc_running(copy1_proc) || is_proc_running(copy2_proc))) {
                        write_copies_still_running(fname);
                    } else {
                        int start_copy1_res = bg_start(&copy1_proc, LOGGER_CMD,
                                                       (char *const[]){LOGGER_CMD, "--mode=copy1", fname, NULL});
                        if (start_copy1_res != 0) {
                            perror("Unable to start copy1");
                            goto cleanup;
                        }
                        int start_copy2_res = bg_start(&copy2_proc, LOGGER_CMD,
                                                       (char *const[]){LOGGER_CMD, "--mode=copy2", fname, NULL});
                        if (start_copy2_res != 0) {
                            perror("Unable to start copy2");
                            goto cleanup;
                        }
                        copies_started = true;
                    }
                }
            } else { // At least one --mode=main is already running --- just wait
                TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
                if (*ref_ctr == 1) { // If all the other mains die, become new main
                    is_origin_inst = true;
                    log_secs = 0;
                    copy_secs = 0;
                }
                TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");
            }
            sleep(IDLE_INTER / 2);
        }
        printf("Log writing finished.\n");
    } else if (logger_mode == MODE_COPY1) {
        TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
        write_start_copy(fname, 1);
        *ctr += 10;
        write_exit_copy(fname, 1);
        TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");
    } else {
        TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
        write_start_copy(fname, 2);
        *ctr *= 2;
        TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");

        sleep(COPY2_DELAY);

        TRY_OR_CLEANUP(sem_wait(sem), "Unable to wait for semaphore");
        *ctr /= 2;
        write_exit_copy(fname, 2);
        TRY_OR_CLEANUP(sem_post(sem), "Unable to post semaphore");
    }

cleanup:
    if (cli_started)
        bg_kill(cli_proc);
    if (sem_loaded) {
        int sem_wait_res = sem_wait(sem);
        assert(sem_wait_res == 0); // If it fails, it fails...

        *ref_ctr -= 1;
        if (*ref_ctr == 0) {
            sem_destroy(sem);
            shm_unlink(shm_name);
        }

        int sem_post_res = sem_post(sem);
        assert(sem_post_res == 0);
    }
    if (mmapped)
        munmap(map_res, LOGGER_SHM_LEN);
    if (shm_opened)
        close(shm_fd);

    return exit_code;
}

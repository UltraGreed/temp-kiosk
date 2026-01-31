#include "logger.h"

#include <stdio.h>

#include "my_types.h"
#include "cross_mem.h"
#include "string.h"
#include "stdlib.h"

#define BUF_LEN 256

int main(void) {
    bool shm_opened = false, mmapped = false, sem_opened = false;
    int exit_code = 0;

    bool shm_existed = mem_shm_exists(LOGGER_SHM_NAME);
    if (shm_existed != 1) {
        perror("Unable to access shared memory from cli child");
        exit_code = 1;
        goto cleanup;
    } 
    MEM_SHM_FD shm_fd = mem_shm_open(LOGGER_SHM_NAME, LOGGER_SHM_LEN);
    shm_opened = true;

    void *addr = mem_mmap(shm_fd, LOGGER_SHM_LEN);
    if (addr == (void *) -1) {
        perror("Unable to map shared memory");
        exit_code = 1;
        goto cleanup;
    }
    mmapped = true;

    i32 *ctr = (i32 *) addr + 1;

    MEM_SEM sem = mem_sem_open(LOGGER_SEM_NAME, 1);
    if (sem == (void *) -1) {
        perror("Unable to open semaphore");
        exit_code = 1;
        goto cleanup;
    }
    sem_opened = true;

    char buf[BUF_LEN];
    while (true) {
        printf("Input exactly one digit to set counter to: \n");
        char *fgets_res = fgets(buf, BUF_LEN, stdin);
        if (fgets_res == NULL) {
            perror("Unable to read stdin");
            exit(1);
        }
        if (strlen(buf) == BUF_LEN - 1 && buf[BUF_LEN - 1] != '\n') {  // Too long line
            perror("Too long line inputed");
        } else {
            i32 new_ctr;
            int sscanf_res = sscanf(buf, "%d\n", &new_ctr);

            if (sscanf_res != 1) {  // Wrong input or OOM
                printf("Wrong input or potential out of memory\n");
            } else {
                int sem_wait_res = mem_sem_wait(sem);
                if (sem_wait_res == -1) {
                    perror("Unable to wait for semaphore");
                    exit(1);
                }

                *ctr = new_ctr;

                int sem_post_res = mem_sem_post(sem);
                if (sem_post_res == -1) {
                    perror("Unable to post semaphore");
                    exit(1);
                }
                printf("Successfully set counter to %d.\n", new_ctr);
            }
        }
    }

cleanup:
    if (sem_opened)
        mem_sem_close(sem);
    if (mmapped)
        mem_munmap(addr, LOGGER_SHM_LEN);
    if (shm_opened)
        mem_shm_close(shm_fd);

    return exit_code;
}

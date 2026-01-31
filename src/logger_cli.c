#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include "my_types.h"
#include "logger.h"

#define BUF_LEN 256

int main(void) {
    bool shm_opened = false, mmapped = false;
    int exit_code = 0;

    // ref_ctr counter semaphore
    // [xxxx]  [xxxx]   [xxxx]  
    const char *shm_name = "/temp_kiosk_logger";
    int shm_flag = O_RDWR;

    int shm_fd = shm_open(shm_name, shm_flag, 0);
    if (shm_fd == -1 && errno != EEXIST) {
        perror("Unable to create shared memory");
        exit_code = 1;
        goto cleanup;
    } 
    shm_opened = true;

    int prot = PROT_READ | PROT_WRITE;
    void *map_res = mmap(NULL, LOGGER_SHM_LEN, prot, MAP_SHARED, shm_fd, 0);
    if (map_res == (void *) -1) {
        perror("Unable to map shared memory");
        exit_code = 1;
        goto cleanup;
    }
    mmapped = true;

    i32 *ctr = (i32 *) map_res + 1;
    sem_t *sem = (sem_t *) ((i32 *) map_res + 2);

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
                perror("Wrong input or potential out of memory");
            } else {
                int sem_wait_res = sem_wait(sem);
                if (sem_wait_res == -1) {
                    perror("Unable to wait for semaphore");
                    exit(1);
                }

                *ctr = new_ctr;

                int sem_post_res = sem_post(sem);
                if (sem_post_res == -1) {
                    perror("Unable to post semaphore");
                    exit(1);
                }
                printf("Successfully set counter to %d.\n", new_ctr);
            }
        }
    }

cleanup:
    if (mmapped)
        munmap(map_res, LOGGER_SHM_LEN);
    if (shm_opened)
        shm_unlink(shm_name);

    return exit_code;
}

#include <cross_mem.h>

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "my_types.h"

int is_exist_shared_mem(const char *name)
{
    int shm = shm_open(name, O_CREAT | O_EXCL, 0);
    int res;
    if (shm == -1 && errno == EEXIST)
        res = 1;
    else if (shm == -1)
        res = -1;
    else
        res = 0;

    if (res == 0)
        shm_unlink(name);

    return res;
}

SharedMemory open_shared_mem(const char *name, usize size)
{
    int existed = is_exist_shared_mem(name);
    if (existed == -1)
        return -1;

    int shm_flag = O_RDWR | O_CREAT;
    mode_t shm_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    SharedMemory shm = shm_open(name, shm_flag, shm_mode);

    if (shm == -1)
        return -1;

    if (!existed) {
        if (ftruncate(shm, size) == -1)
            return -1;
    }

    return shm;
}

int close_shared_mem(SharedMemory shm)
{
    return close(shm);
}

int unlink_shared_mem(const char *name)
{
    return shm_unlink(name);
}

void *map_shared_mem(SharedMemory shm, usize size)
{
    int prot = PROT_READ | PROT_WRITE;
    void *addr = mmap(NULL, size, prot, MAP_SHARED, shm, 0);
    return addr;
}

int unmap_shared_mem(void *addr, usize size)
{
    return munmap(addr, size);
}

Semaphore open_semaphore(const char *name, int value)
{
    int sem_flag = O_RDWR | O_CREAT;
    mode_t sem_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    return sem_open(name, sem_flag, sem_mode, value);
}

int close_semaphore(Semaphore sem)
{
    return sem_close(sem);
}

int wait_semaphore(Semaphore sem)
{
    return sem_wait(sem);
}

int post_semaphore(Semaphore sem)
{
    return sem_post(sem);
}

int unlink_semaphore(const char *name)
{
    return sem_unlink(name);
}

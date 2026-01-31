#include <cross_mem.h>

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>

int mem_shm_exists(const char *shm_name) {
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

MEM_SHM_FD mem_shm_open(const char *shm_name, usize shm_size) {
    int existed = mem_shm_exists(shm_name);
    if (existed == -1)
        return -1;

    int shm_flag = O_RDWR | O_CREAT;
    mode_t shm_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    MEM_SHM_FD shm_fd = shm_open(shm_name, shm_flag, shm_mode);

    if (shm_fd == -1)
        return -1;

    if (!existed) {
        if (ftruncate(shm_fd, shm_size) == -1)
            return -1;
    }

    return shm_fd;
}

int mem_shm_close(MEM_SHM_FD shm_fd) {
    return close(shm_fd);
}

int mem_shm_unlink(const char *shm_name) {
    return shm_unlink(shm_name);
}

void *mem_mmap(MEM_SHM_FD shm_fd, usize shm_size) {
    int prot = PROT_READ | PROT_WRITE;
    void *addr = mmap(NULL, shm_size, prot, MAP_SHARED, shm_fd, 0);
    return addr;
}

int mem_munmap(void *addr, usize shm_size) {
    return munmap(addr, shm_size);
}

MEM_SEM mem_sem_open(const char *name, int value) {
    int sem_flag = O_RDWR | O_CREAT;
    mode_t sem_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    return sem_open(name, sem_flag, sem_mode, value);
}

int mem_sem_close(MEM_SEM sem) {
    return sem_close(sem);
}

int mem_sem_wait(MEM_SEM sem) {
    return sem_wait(sem);
}

int mem_sem_post(MEM_SEM sem) {
    return sem_post(sem);
}

int mem_sem_unlink(const char *name) {
    return sem_unlink(name);
}

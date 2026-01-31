#pragma once

#ifdef __linux__ 
#include <semaphore.h>
typedef int MEM_SHM_FD;
typedef sem_t *MEM_SEM;
#else
#include <windows.h>
typedef HANDLE MEM_SHM_FD;
typedef HANDLE MEM_SEM;
#endif

#include "my_types.h"


/// Return -1 on error, 1 if shared memory exists, 0 otherwise
int mem_shm_exists(const char *shm_name);

/// Open shared memory
/// Return (MEM_SHM) -1 on error, address to fd otherwise
MEM_SHM_FD mem_shm_open(const char *shm_name, usize shm_size);

/// Close shared memory
/// Return -1 on error, 0 otherwise
int mem_shm_close(MEM_SHM_FD shm_fd);

/// Delete shared memory file
/// Return -1 on error, 0 otherwise
int mem_shm_unlink(const char *shm_name);

/// Map shared memory to local memory
/// Return (void *) -1 on error, address otherwise
void *mem_mmap(MEM_SHM_FD shm_fd, usize shm_size);

/// Unmap mapped shared memory 
/// Return -1 on error, 0 otherwise
int mem_munmap(void *addr, usize shm_size);

/// Open named semaphore or init it with initial value
/// Return (MEM_SEM) -1 on error, 0 otherwise
MEM_SEM mem_sem_open(const char *name, int value);

/// Close named semaphore
/// Return (void *) -1 on error, 0 otherwise
int mem_sem_close(MEM_SEM sem);

/// Reduce semaphore value by 1 if it's positive, wait and reduce otherwise
/// Return -1 on error, 0 otherwise
int mem_sem_wait(MEM_SEM mem_sem);

/// Increase semaphore value by 1
/// Return -1 on error, 0 otherwise
int mem_sem_post(MEM_SEM mem_sem);

/// Unlink named semaphore
/// Return -1 on error, 0 otherwise
int mem_sem_unlink(const char *name);

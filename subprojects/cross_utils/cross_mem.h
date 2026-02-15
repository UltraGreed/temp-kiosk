#pragma once

#ifdef WIN32
#include <windows.h>
typedef HANDLE SharedMemory;
typedef HANDLE Semaphore;
#else
#include <semaphore.h>
typedef int SharedMemory;
typedef sem_t *Semaphore;
#endif

#include "my_types.h"
#include "utils.h"

/// Return -1 on error, 1 if shared memory exists, 0 otherwise
int is_exist_shared_mem(const char *name);

/// Open shared memory
/// Return (MEM_SHM) -1 on error, address to fd otherwise
SharedMemory open_shared_mem(const char *name, usize size);

/// Close shared memory
/// Return -1 on error, 0 otherwise
int close_shared_mem(SharedMemory shm);

/// Delete shared memory file
/// Return -1 on error, 0 otherwise
int unlink_shared_mem(const char *name);

/// Map shared memory to local memory
/// Return (void *) -1 on error, address otherwise
void *map_shared_mem(SharedMemory shm, usize size);

/// Unmap mapped shared memory
/// Return -1 on error, 0 otherwise
int unmap_shared_mem(void *addr, usize shm_size);

/// Open named semaphore or init it with initial value
/// Return (MEM_SEM) -1 on error, 0 otherwise
Semaphore open_semaphore(const char *name, int value);

/// Close named semaphore
/// Return (void *) -1 on error, 0 otherwise
int close_semaphore(Semaphore sem);

/// Reduce semaphore value by 1 if it's positive, wait and reduce otherwise
/// Return -1 on error, 0 otherwise
int wait_semaphore(Semaphore sem);

/// Increase semaphore value by 1
/// Return -1 on error, 0 otherwise
int post_semaphore(Semaphore sem);

/// Unlink named semaphore
/// Return -1 on error, 0 otherwise
int unlink_semaphore(const char *name);

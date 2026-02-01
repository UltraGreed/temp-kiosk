#include "cross_mem.h"

#include <errhandlingapi.h>
#include <handleapi.h>
#include <memoryapi.h>
#include <minwindef.h>
#include <stdbool.h>
#include <synchapi.h>
#include <windows.h>
#include <winerror.h>
#include <winnt.h>

#include "my_types.h"

int mem_shm_exists(const char *shm_name) {
    MEM_SHM_FD *new_shm_fd = mem_shm_open(shm_name, 1);

    if (new_shm_fd == (MEM_SHM_FD *)-1)
        return -1;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return true;

    if (mem_shm_unlink(shm_name) == -1)
        return -1;

    return false;
}

MEM_SHM_FD mem_shm_open(const char *shm_name, usize shm_size) {
    HANDLE new_mem_shm = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, shm_size, shm_name);

    if (new_mem_shm == NULL)
        return (void *)-1;

    return new_mem_shm;
}

int mem_shm_close(MEM_SHM_FD shm_fd) {
    return CloseHandle(shm_fd) == 0 ? -1 : 0; // This API is bullshit
}

int mem_shm_unlink(const char *shm_name) { }

void *mem_mmap(MEM_SHM_FD shm_fd, usize shm_size) {
    LPTSTR new_map = MapViewOfFile(shm_fd, FILE_MAP_ALL_ACCESS, 0, 0, shm_size);
    if (new_map == NULL)
        return (void *)-1;

    return new_map;
}

int mem_munmap(void *addr, usize shm_size) {
    return UnmapViewOfFile(addr);
}

MEM_SEM mem_sem_open(const char *name, int value) {
    MEM_SEM sem = CreateSemaphore(NULL, value, value, name);
    if (sem == NULL)
        return (void *)-1;

    return sem;
}

int mem_sem_close(MEM_SEM sem) {
    return CloseHandle(sem) == 0 ? -1 : 0; // This API is bullshit
}

int mem_sem_wait(MEM_SEM mem_sem) {
    DWORD wait_res = WaitForSingleObject(mem_sem, INFINITE);

    if (wait_res == (DWORD)-1)
        return -1;

    return 0;
}

int mem_sem_post(MEM_SEM mem_sem) {
    return ReleaseSemaphore(mem_sem, 1, NULL) == 0 ? -1 : 0; // This API is bullshit
}

int mem_sem_unlink(const char *name) {}

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
#include "utils.h"

int is_exist_shared_mem(const char *name)
{
    SharedMemory *new_shm_fd = open_shared_mem(name, 1);

    if (new_shm_fd == (SharedMemory *)-1)
        return -1;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return true;

    if (unlink_shared_mem(name) == -1)
        return -1;

    return false;
}

SharedMemory open_shared_mem(const char *name, usize size)
{
    HANDLE new_shm = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);

    if (new_shm == NULL)
        return (void *)-1;

    return new_shm;
}

int close_shared_mem(SharedMemory shm)
{
    return CloseHandle(shm) == 0 ? -1 : 0; // This API is bullshit
}

int unlink_shared_mem(const char *name)
{
    (void) name;
    return 0;
}

void *map_shared_mem(SharedMemory shm, usize size)
{
    LPTSTR new_map = MapViewOfFile(shm, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (new_map == NULL)
        return (void *)-1;

    return new_map;
}

int unmap_shared_mem(void *addr, usize size)
{
    (void) size;
    return UnmapViewOfFile(addr);
}

Semaphore open_semaphore(const char *name, int value)
{
    Semaphore sem = CreateSemaphore(NULL, value, value, name);
    if (sem == NULL)
        return (void *)-1;

    return sem;
}

int close_semaphore(Semaphore sem)
{
    return CloseHandle(sem) == 0 ? -1 : 0; // This API is bullshit
}

int wait_semaphore(Semaphore mem_sem)
{
    DWORD wait_res = WaitForSingleObject(mem_sem, INFINITE);

    if (wait_res == (DWORD)-1)
        return -1;

    return 0;
}

int post_semaphore(Semaphore mem_sem)
{
    return ReleaseSemaphore(mem_sem, 1, NULL) == 0 ? -1 : 0; // This API is bullshit
}

int unlink_semaphore(const char *name)
{
    (void)name;
    return 0;
}

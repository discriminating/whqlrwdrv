#pragma once

#include <windows.h>
#include <stdio.h>

#define DRV_NAMEW                L"drv.sys"
#define IOCTL_BASE               0x1120E040
#define IOCTL_READ               0x1120E048

#define DRV_STATUS_LOAD_FAILED   -1
#define DRV_STATUS_CANT_WRITE    -2
#define DRV_STATUS_CANT_PIPE     -3
#define DRV_STATUS_CHECKS_FAILED -4

typedef struct _GET_BASE_ADDR {
    DWORD pid;
    PVOID addr;
} GET_BASE_ADDR, *PGET_BASE_ADDR;

typedef struct _READ_MEM {
    DWORD pid;
    PVOID buff;
    PVOID addr;
    DWORD size;
} READ_MEM, *PREAD_MEM;

int     initDrv();
BOOL    dropDrv();
BOOL    loadDrv();
BOOL    unloadDrv();
LPCWSTR getDrvPath();

BOOL drvReadMemory(HANDLE hDrv, DWORD pid, PVOID addr, void* buffer, DWORD size);
BOOL drvWriteMemory(HANDLE hDrv, DWORD pid, PVOID addr, void* buffer, DWORD size);
BOOL drvGetBaseAddr(HANDLE hDrv, DWORD pid, PVOID* addr);

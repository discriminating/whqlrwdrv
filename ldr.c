#include "drv/drv.h"

int main()
{
    HANDLE drv = initDrv();

    if (drv == INVALID_HANDLE_VALUE || drv < 0)
    {
        printf("Failed to load driver.\n");
        return 1;
    }
    
    printf("Driver loaded\n");

    int value = 4;
    int newValue = 1337;

    void* buffer = malloc(4);
    if (drvReadMemory(drv, GetCurrentProcessId(), (PVOID)&value, buffer, 4))
    {
        if (buffer != NULL)
            printf("Read value: %d\n", *(int*)buffer);
    }

    if (drvWriteMemory(drv, GetCurrentProcessId(), (PVOID)&value, &newValue, 4))
        printf("Wrote value: %d\n", newValue);

    printf("Value: %d\n", value);
    printf("New value: %d\n", newValue);

    if (newValue == value)
        printf("Success: kernel can write and read\n");
    else
    {
        printf("Failed: kernel can't write and read\n");

        return DRV_STATUS_CHECKS_FAILED;
    }

    DWORD64 lsassBase = 0;
    if (drvGetBaseAddr(drv, 1840, (PVOID*)&lsassBase))
        printf("lsass.exe base address: %llx\n", lsassBase);

    BOOL unload = unloadDrv();

    if (!unload)
    {
        printf("Failed to unload driver.\n");
        return 1;
    }

    printf("Driver unloaded.\n");

    return 0;
}


#include <memory.h>
#include <string.h>

#include "hal.h"
#include "global.h"
#include "setting.h"

static struct sys_setup_t SetupStore;
extern const char git_ver[];
extern const char git_info[];

void Sys_SetInit(void)
{
    const struct sys_setup_t *SysSetup;
    SysSetup = (const struct sys_setup_t *) USER_DATA_ADDR;
    if (SysSetup->tag != FLAG_TAG) {
        DPRINTF(("Run for first time....\n"));
        Sys_SetDefault();
    } else {
        if (SysSetup->bootFlag  == FLAG_UPDONE) {
            Sys_CleanupUpgrade();
        }
    }
}

void Sys_GetVer(char *ver)
{
    const struct sys_setup_t *SysSetup;
    SysSetup = (const struct sys_setup_t *) USER_DATA_ADDR;
    if (strlen(SysSetup->appVersion) < 64) {
        strcpy(ver, SysSetup->appVersion);
    } else {
        strcpy(ver, "0.00");
    }

    return ;
}

void Sys_CleanupUpgrade(void)
{
    const struct sys_setup_t *SysSetup;
    SysSetup = (const struct sys_setup_t *) USER_DATA_ADDR;

    memcpy((void *)&SetupStore, (void *)USER_DATA_ADDR, sizeof(struct sys_setup_t));
    SetupStore.bootFlag = 0;
    if (strlen(git_ver) + strlen(git_info) > 63) {
        strcpy(SetupStore.appVersion, "0.00");
    } else {
        sprintf(SetupStore.appVersion, "%s.%s", git_ver, git_info);
    }

    DPRINTF(("Upgrade new app, version %s.\n", SetupStore.appVersion));

    if (memcmp((void *)SysSetup, (void *)&SetupStore, sizeof(struct sys_setup_t)) == 0) return ;
    if (FLASH_If_EraseData() == 0) {
        FLASH_If_Write((uint32_t) USER_DATA_ADDR, (uint32_t *) &SetupStore, (uint16_t)((sizeof(struct sys_setup_t) + 3) / 4));
    }

}

void Sys_SetDefault(void)
{
    const struct sys_setup_t *SysSetup;
    SysSetup = (const struct sys_setup_t *) USER_DATA_ADDR;

    SetupStore.tag = FLAG_TAG;
    SetupStore.bootFlag = 0;
    SetupStore.UsrOffset = 0;
    if (strlen(git_ver) + strlen(git_info) > 63) {
        strcpy(SetupStore.appVersion, "0.00");
    } else {
        sprintf(SetupStore.appVersion, "%s.%s", git_ver, git_info);
    }

    if (memcmp((void *)SysSetup, (void *)&SetupStore, sizeof(struct sys_setup_t)) == 0) return ;
    if (FLASH_If_EraseData() == 0) {
        FLASH_If_Write((uint32_t) USER_DATA_ADDR, (uint32_t *) &SetupStore, (uint16_t)((sizeof(struct sys_setup_t) + 3) / 4));
    }
}

int32_t Sys_SetBootFlag(uint32_t flag)
{
    uint32_t result;

    memcpy((void *)&SetupStore, (void *)USER_DATA_ADDR, sizeof(struct sys_setup_t));
    result = FLASH_If_EraseData();
    if (result) {
        DPRINTF(("faile to erase flash.\n"));
        return -1;
    }
    SetupStore.bootFlag = flag;
    result = FLASH_If_Write((uint32_t)USER_DATA_ADDR, (uint32_t *) &SetupStore, (uint16_t)((sizeof(struct sys_setup_t) + 3) / 4));
    if (result) return -1;
    else
        return 0;
}

int32_t Sys_Set2BL(void)
{
    return Sys_SetBootFlag(FLAG_BOOTCODE);
}

int32_t Sys_Set2App(void)
{
    return Sys_SetBootFlag(FLAG_UPDONE);
}

int16_t Sys_GetUsrOffset(void)
{
    const struct sys_setup_t *SysSetup;
    SysSetup = (const struct sys_setup_t *) USER_DATA_ADDR;
    return SysSetup->UsrOffset;
}

int32_t Sys_SetUsrOffset(int16_t setvalue)
{
    uint32_t result = 0;
    const struct sys_setup_t *SysSetup;
    SysSetup = (const struct sys_setup_t *) USER_DATA_ADDR;
    if (setvalue == SysSetup->UsrOffset) return 0;

    memcpy((void *)&SetupStore, (void *)USER_DATA_ADDR, sizeof(struct sys_setup_t));
    SetupStore.UsrOffset = setvalue;

    if (FLASH_If_EraseData() == 0) {
        result =  FLASH_If_Write((uint32_t) USER_DATA_ADDR, (uint32_t *) &SetupStore, (uint16_t)((sizeof(struct sys_setup_t) + 3) / 4));
        if (result) return -1;
        else return 0;
    } else {
        return -1;
    }

    return 0;
}


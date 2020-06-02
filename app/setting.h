
#ifndef __SETTING_H__
#define __SETTING_H__

#include "flash_if.h"

#define USER_DATA_ADDR  USER_START_ADDRESS

#define FLAG_BOOTCODE   ((uint32_t)0x3238face)
#define FLAG_TAG        ((uint32_t)0x2588900d)
#define FLAG_UPDONE     ((uint32_t)0x38383838)

struct sys_setup_t {
    uint32_t tag;
    uint32_t bootFlag;
    char appVersion[64 + 4];
    int16_t UsrOffset;
    int16_t res0;
    int32_t r0[32];
};

int32_t Sys_Set2BL(void);
int32_t Sys_Set2App(void);
void Sys_SetDefault(void);
void Sys_SetInit(void);
void Sys_CleanupUpgrade(void);
void Sys_GetVer(char *ver);
int16_t Sys_GetUsrOffset(void);
int32_t Sys_SetUsrOffset(int16_t setvalue);

#endif



#define MAIN

#include <stdio.h>
#include "hal.h"
#include "global.h"
#include "comslave.h"
#include "setting.h"
#include "htpa32_main.h"
#include "ver.h"

const char build_date[] = __DATE__;
const char build_time[] = __TIME__;
const char compile_by[] = COMPILE_BY;
const char compile_at[] = COMPILE_AT;
const char compiler[] = COMPILER ;
const char version_chk[] = VERSION_CHECK ;
const char git_ver[] = GIT_VER;
const char git_info[] = GIT_INFO;

int main(void)
{
    Hal_Init();

    DPRINTF(("Booting ......\n"));
    DPRINTF(("Firmware version %s.%s\n", git_ver, git_info));
    DPRINTF(("%s version.\n", version_chk));
    DPRINTF(("Compiled by %s@%s.\n", compile_by, compile_at));
    DPRINTF(("xiangww00@yahoo.com.\n"));
    DPRINTF(("%s\n", compiler));
    DPRINTF(("Build Time: %s %s \n", build_date, build_time));
    DPRINTF(("SystemCoreClock =%d \n", SystemCoreClock));

    Sys_SetInit();
    ComSlave_Init();
    Htpa32_Init();

    IWDG_Config();
    do {

        Htpa32_Service();
        ComSlave_Service();

        IWDG_ReloadCounter();
    } while (1);

    return 0;
}


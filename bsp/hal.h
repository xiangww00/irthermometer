
#ifndef __HAL_H__
#define __HAL_H__

#include "global.h"

void Hal_Init(void);
void Delay(uint32_t nCount);
void DelayUS(uint32_t usec);
void DelayMS(uint32_t msec);

void IWDG_Config(void);

void Sensor_Ctrl_Init(void);
void Sensor_Ctrl_on(void);
void Sensor_Ctrl_off(void);

static inline ticks_t Hal_GetTicks(void)
{
    return SysTicks;
}


#endif


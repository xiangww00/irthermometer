
#include "common.h"
#include "hal.h"
#include "i2c_drv.h"
#include "usart.h"

void Hal_Init(void)
{
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);
    Usart_Debug_Init();
}

void Delay(uint32_t nCount)
{
    do {
        __asm volatile("nop");
    } while (nCount --);
}

void DelayUS(uint32_t usec)
{
    usec = usec * 100;
    do {
        __asm volatile("nop");
    } while (usec --);
    return;
}

void DelayMS(uint32_t msec)
{
    ticks_t toms;
    if (msec < 3) {
        DelayUS(msec * 1000);
        return ;
    }
    toms = (ticks_t) msec + Hal_GetTicks();
    while (Hal_GetTicks() < toms);
}

void IWDG_Config(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_32);
    IWDG_SetReload(32000 / 32);
    IWDG_ReloadCounter();
    IWDG_Enable();
}


#ifndef BOOTLOADER
#define SENSOR_CTRL_PIN GPIO_Pin_8
void Sensor_Ctrl_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP ;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Sensor_Ctrl_on(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
}

void Sensor_Ctrl_off(void)

{
    GPIO_SetBits(GPIOB, GPIO_Pin_8);
}
#endif


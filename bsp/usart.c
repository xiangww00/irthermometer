
#include <string.h>

#include "stm32f4xx.h"
#include "global.h"
#include "usart.h"
#include "hal.h"

#define USART_TX_TIMEOUT 0x1000

#define COMM_TX_SIZE 1552
#define COMM_RX_SIZE 1552

static uint8_t comm_txbuf[COMM_TX_SIZE] __attribute__((aligned(4)));
static uint8_t comm_rxbuf[COMM_RX_SIZE] __attribute__((aligned(4)));

typedef void (*TxFunc)(int32_t);

static volatile int8_t tx_busy[3] = {0};
static uint16_t Rx_BufSize[3] = { 0 };
static uint32_t Rx_BufStart[3] = { 0 };
static int32_t RdPtr_Rx[3] = { 0 };
const TxFunc Usart_TxFunc[3] = {
    Usart1_Tx,
    Usart2_Tx,
    Usart6_Tx,
};

struct UsartCtrl_t {
    USART_TypeDef *usart;
    uint32_t clk;
    DMA_TypeDef *dma;

    uint32_t tx_gpio_clk;
    GPIO_TypeDef *tx_GPIO;
    uint8_t tx_PinSource;
    uint8_t tx_AF;
    uint16_t tx_Pin;
    uint32_t tx_DMA_Channel;
    uint32_t tx_DMA_Flag;
    uint32_t tx_IT_Flag;
    int32_t tx_IRQn;
    DMA_Stream_TypeDef *tx_DMA_Stream;

    uint32_t rx_gpio_clk;
    GPIO_TypeDef *rx_GPIO;
    uint8_t rx_PinSource;
    uint8_t rx_AF;
    uint16_t rx_Pin;

    uint32_t rx_DMA_Channel;
    uint32_t rx_DMA_Flag;
    DMA_Stream_TypeDef *rx_DMA_Stream;

};

const struct UsartCtrl_t myUsartCtrl[PORT_TOTAL] = {
    {
        .usart = USART1,
        .clk = RCC_APB2Periph_USART1,
        .dma = DMA2,

        .tx_gpio_clk = RCC_AHB1Periph_GPIOA,
        .tx_GPIO = GPIOA,
        .tx_PinSource = GPIO_PinSource9,
        .tx_Pin = GPIO_Pin_9,
        .tx_AF = GPIO_AF_USART1,

        .tx_DMA_Stream = DMA2_Stream7,
        .tx_DMA_Channel = DMA_Channel_4,
        .tx_DMA_Flag = DMA_FLAG_HTIF7 | DMA_FLAG_TCIF7,
        .tx_IT_Flag = DMA_IT_TCIF7,
        .tx_IRQn = DMA2_Stream7_IRQn,

        .rx_gpio_clk = RCC_AHB1Periph_GPIOA,
        .rx_GPIO = GPIOA,
        .rx_PinSource = GPIO_PinSource10,
        .rx_Pin = GPIO_Pin_10,
        .rx_AF = GPIO_AF_USART1,

        .rx_DMA_Stream = DMA2_Stream2,
        .rx_DMA_Channel = DMA_Channel_4,
        .rx_DMA_Flag = DMA_FLAG_FEIF2 | DMA_FLAG_DMEIF2 | DMA_FLAG_TEIF2  | DMA_FLAG_HTIF2 | DMA_FLAG_TCIF2,
    },
    {
        .usart = USART2,
        .clk = RCC_APB1Periph_USART2,
        .dma = DMA1,

        .tx_gpio_clk = RCC_AHB1Periph_GPIOA,
        .tx_GPIO = GPIOA,
        .tx_PinSource = GPIO_PinSource2,
        .tx_Pin = GPIO_Pin_2,
        .tx_AF = GPIO_AF_USART2,

        .tx_DMA_Stream = DMA1_Stream6,
        .tx_DMA_Channel = DMA_Channel_4,
        .tx_DMA_Flag = DMA_FLAG_HTIF6 | DMA_FLAG_TCIF6,
        .tx_IT_Flag = DMA_IT_TCIF6,
        .tx_IRQn = DMA1_Stream6_IRQn,

        .rx_gpio_clk = RCC_AHB1Periph_GPIOA,
        .rx_GPIO = GPIOA,
        .rx_PinSource = GPIO_PinSource3,
        .rx_Pin = GPIO_Pin_3,
        .rx_AF = GPIO_AF_USART2,

        .rx_DMA_Channel = DMA_Channel_4,
        .rx_DMA_Stream = DMA1_Stream5,
        .rx_DMA_Flag = DMA_FLAG_FEIF5 | DMA_FLAG_DMEIF5 | DMA_FLAG_TEIF5  | DMA_FLAG_HTIF5 | DMA_FLAG_TCIF5,
    },
    {
        .usart = USART6,
        .clk = RCC_APB2Periph_USART6,
        .dma = DMA2,

        .tx_gpio_clk = RCC_AHB1Periph_GPIOA,
        .tx_GPIO = GPIOA,
        .tx_PinSource = GPIO_PinSource11,
        .tx_Pin = GPIO_Pin_11,
        .tx_AF = GPIO_AF_USART6,

        .tx_DMA_Stream = DMA2_Stream6,
        .tx_DMA_Channel = DMA_Channel_5,
        .tx_DMA_Flag = DMA_FLAG_HTIF6 | DMA_FLAG_TCIF6,
        .tx_IT_Flag = DMA_IT_TCIF6,
        .tx_IRQn = DMA2_Stream6_IRQn,

        .rx_gpio_clk = RCC_AHB1Periph_GPIOA,
        .rx_GPIO = GPIOA,
        .rx_PinSource = GPIO_PinSource12,
        .rx_Pin = GPIO_Pin_12,
        .rx_AF = GPIO_AF_USART6,

        .rx_DMA_Channel = DMA_Channel_5,
        .rx_DMA_Stream = DMA2_Stream1,
        .rx_DMA_Flag = DMA_FLAG_FEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_TEIF1  | DMA_FLAG_HTIF1 | DMA_FLAG_TCIF1,
    },
};


void General_Port_Init(int8_t port, USART_InitTypeDef *USART_InitStruct);
void Usart_Tx_dma_init(int port, uint32_t tx_buf, uint16_t tx_size);
void Usart_Rx_dma_init(int port, uint32_t rx_buf, uint16_t rx_size);

int __io_putchar(int ch)
{
    USART_SendData(myUsartCtrl[DEBUG_PORT].usart, ch);
    while (USART_GetFlagStatus(myUsartCtrl[DEBUG_PORT].usart, USART_FLAG_TC) == RESET);
    return ch;
}

void Usart_Debug_Init(void)
{
    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    General_Port_Init(DEBUG_PORT, &USART_InitStructure);
    return ;
}


void General_Port_Init(int8_t port, USART_InitTypeDef *pUSART_InitStruct)
{
    USART_TypeDef *Usart;
    GPIO_InitTypeDef GPIO_InitStructure;
    const struct UsartCtrl_t *pCtrl;

    if (port < 0 || port > PORT_TOTAL) {
        return ;
    }

    pCtrl = &myUsartCtrl[port];
    Usart = pCtrl->usart;

    if (pCtrl->usart == USART2) {
        RCC_APB1PeriphClockCmd(pCtrl->clk, ENABLE);
    } else {
        RCC_APB2PeriphClockCmd(pCtrl->clk, ENABLE);
    }

    USART_DeInit(Usart);

    RCC_AHB1PeriphClockCmd(pCtrl->tx_gpio_clk | pCtrl->rx_gpio_clk, ENABLE);

    GPIO_PinAFConfig(pCtrl->tx_GPIO, pCtrl->tx_PinSource, pCtrl->tx_AF);
    GPIO_PinAFConfig(pCtrl->rx_GPIO, pCtrl->rx_PinSource, pCtrl->rx_AF);

    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;

    GPIO_InitStructure.GPIO_Pin = pCtrl->tx_Pin;
    GPIO_Init(pCtrl->tx_GPIO, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = pCtrl->rx_Pin;
    GPIO_Init(pCtrl->rx_GPIO, &GPIO_InitStructure);

    USART_Init(Usart, pUSART_InitStruct);
    USART_ClearFlag(Usart, USART_FLAG_TC);
    USART_Cmd(Usart, ENABLE);
    return ;
}

void Usart_Tx_dma_init(int port, uint32_t tx_buf, uint16_t tx_size)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure_Tx;

    const struct UsartCtrl_t *pCntrl;
    USART_TypeDef *Usart;

    if (port < 0 || port > PORT_TOTAL) {
        EPRINTF(("Invalid port. %d Line %d \n", port, __LINE__));
        return ;
    }

    pCntrl = &myUsartCtrl[port];
    Usart = pCntrl->usart;

    if (pCntrl->dma == DMA1) {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    } else {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    }

    DMA_Cmd(pCntrl->tx_DMA_Stream, DISABLE);
    DMA_DeInit(pCntrl->tx_DMA_Stream);

    DMA_InitStructure_Tx.DMA_PeripheralBaseAddr = (uint32_t)(&(Usart->DR));
    DMA_InitStructure_Tx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure_Tx.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure_Tx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure_Tx.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure_Tx.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure_Tx.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure_Tx.DMA_FIFOMode = DMA_FIFOMode_Disable  ;
    DMA_InitStructure_Tx.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure_Tx.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure_Tx.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

    DMA_InitStructure_Tx.DMA_Channel = pCntrl->tx_DMA_Channel;
    DMA_InitStructure_Tx.DMA_DIR = DMA_DIR_MemoryToPeripheral  ;

    DMA_InitStructure_Tx.DMA_Memory0BaseAddr = tx_buf;

    NVIC_InitStructure.NVIC_IRQChannel =  pCntrl->tx_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    DMA_ClearFlag(pCntrl->tx_DMA_Stream, pCntrl->tx_DMA_Flag);
    DMA_ClearITPendingBit(pCntrl->tx_DMA_Stream , pCntrl->tx_IT_Flag);

    DMA_ITConfig(pCntrl->tx_DMA_Stream, DMA_IT_TC, ENABLE);

    DMA_InitStructure_Tx.DMA_BufferSize = tx_size;
    DMA_Init(pCntrl->tx_DMA_Stream, &DMA_InitStructure_Tx);
    DMA_Cmd(pCntrl->tx_DMA_Stream, DISABLE);

    return ;
}

void Usart_Rx_dma_init(int port, uint32_t rx_buf, uint16_t rx_size)
{
    DMA_InitTypeDef  DMA_InitStructure_Rx;

    const struct UsartCtrl_t *pCntrl;
    USART_TypeDef *Usart;

    if (port < 0 || port > PORT_TOTAL) {
        EPRINTF(("Invalid port. %d Line %d \n", port, __LINE__));
        return ;
    }

    pCntrl = &myUsartCtrl[port];
    Usart = pCntrl->usart;
    Rx_BufSize[port] = rx_size;
    Rx_BufStart[port] = rx_buf;
    RdPtr_Rx[port] = 0;

    if (pCntrl->dma == DMA1) {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    } else {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    }

    DMA_Cmd(pCntrl->rx_DMA_Stream, DISABLE);
    DMA_DeInit(pCntrl->rx_DMA_Stream);

    DMA_InitStructure_Rx.DMA_PeripheralBaseAddr = (uint32_t)(&(Usart->DR));
    DMA_InitStructure_Rx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure_Rx.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure_Rx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure_Rx.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure_Rx.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure_Rx.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure_Rx.DMA_FIFOMode = DMA_FIFOMode_Disable  ;
    DMA_InitStructure_Rx.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure_Rx.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure_Rx.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

    DMA_InitStructure_Rx.DMA_Channel = pCntrl->rx_DMA_Channel;
    DMA_InitStructure_Rx.DMA_DIR = DMA_DIR_PeripheralToMemory;

    DMA_InitStructure_Rx.DMA_Memory0BaseAddr = rx_buf;
    DMA_InitStructure_Rx.DMA_BufferSize = rx_size;

    DMA_Init(pCntrl->rx_DMA_Stream, &DMA_InitStructure_Rx);
    DMA_ClearFlag(pCntrl->rx_DMA_Stream, pCntrl->rx_DMA_Flag);

    USART_DMACmd(Usart, USART_DMAReq_Rx, ENABLE);
    DMA_Cmd(pCntrl->rx_DMA_Stream, ENABLE);

    return ;
}

void Usart6_Tx(int32_t len)
{
    DMA_SetCurrDataCounter(DMA2_Stream6, len);

    USART_DMACmd(USART6, USART_DMAReq_Tx, ENABLE);
    USART_ClearFlag(USART6, USART_FLAG_TC);

    DMA_Cmd(DMA2_Stream6, ENABLE);
    tx_busy[PORT_COM6] = 1;

    return ;
}

void DMA2_Stream7_IRQHandler(void)
{
    volatile uint32_t Timeout_isr  = 0;
    DMA_Cmd(DMA2_Stream7, DISABLE);
    USART_DMACmd(USART1, USART_DMAReq_Tx, DISABLE);

    DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_HTIF7 | DMA_FLAG_TCIF7);
    DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);

    Timeout_isr = USART_TX_TIMEOUT;
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {
        if ((Timeout_isr--) == 0) break;
    }

    tx_busy[PORT_COM1] = 0;
    return ;
}

void Usart1_Tx(int32_t len)
{
    DMA_SetCurrDataCounter(DMA2_Stream7, len);

    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_ClearFlag(USART1, USART_FLAG_TC);

    DMA_Cmd(DMA2_Stream7, ENABLE);
    tx_busy[PORT_COM1] = 1;
    return ;
}

void DMA2_Stream6_IRQHandler(void)
{
    volatile uint32_t Timeout_isr  = 0;
    DMA_Cmd(DMA2_Stream6, DISABLE);
    USART_DMACmd(USART6, USART_DMAReq_Tx, DISABLE);

    DMA_ClearFlag(DMA2_Stream6, DMA_FLAG_HTIF6 | DMA_FLAG_TCIF6);
    DMA_ClearITPendingBit(DMA2_Stream6, DMA_IT_TCIF6);

    Timeout_isr = USART_TX_TIMEOUT;
    while (USART_GetFlagStatus(USART6, USART_FLAG_TC) == RESET) {
        if ((Timeout_isr--) == 0) break;
    }

    tx_busy[PORT_COM6] = 0;
    return ;
}

void Usart2_Tx(int32_t len)
{
    DMA_SetCurrDataCounter(DMA1_Stream6, len);

    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
    USART_ClearFlag(USART2, USART_FLAG_TC);

    DMA_Cmd(DMA1_Stream6, ENABLE);
    tx_busy[PORT_COM2] = 1;
    return ;
}

void DMA1_Stream6_IRQHandler(void)
{
    volatile uint32_t Timeout_isr  = 0;
    DMA_Cmd(DMA1_Stream6, DISABLE);
    USART_DMACmd(USART2, USART_DMAReq_Tx, DISABLE);

    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_HTIF6 | DMA_FLAG_TCIF6);
    DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);

    Timeout_isr = USART_TX_TIMEOUT;
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET) {
        if ((Timeout_isr--) == 0) break;
    }

    tx_busy[PORT_COM2] = 0;
    return ;
}


int32_t Usart_Get_RxSize(int8_t port)
{
    int32_t fullness;
    int32_t WrPtr_Rx = 0;
    const struct UsartCtrl_t *pCntrl;

    if (port < 0 || port > PORT_TOTAL) {
        EPRINTF(("Invalid port. %d Line %d \n", port, __LINE__));
        return 0 ;
    }

    pCntrl = &myUsartCtrl[port];

    WrPtr_Rx = (int32_t)(((uint32_t) Rx_BufSize[port]) - ((uint32_t)((pCntrl->rx_DMA_Stream)->NDTR))) ;
    fullness = WrPtr_Rx - RdPtr_Rx[port];
    if (fullness < 0) fullness += Rx_BufSize[port];
    return fullness;
}

void Usart_Reset_Rx(int8_t port)
{
    int32_t WrPtr_Rx = 0;
    const struct UsartCtrl_t *pCntrl;

    if (port < 0 || port > PORT_TOTAL) {
        EPRINTF(("Invalid port. %d Line %d \n", port, __LINE__));
        return ;
    }

    pCntrl = &myUsartCtrl[port];

    WrPtr_Rx = (int32_t)(((uint32_t) Rx_BufSize[port]) - ((uint32_t)((pCntrl->rx_DMA_Stream)->NDTR))) ;

    RdPtr_Rx[port] = WrPtr_Rx ;
    return ;
}

int32_t Usart_Read(int8_t port, uint8_t *buf, int32_t size)
{
    int32_t fullness;
    int32_t WrPtr_Rx = 0;
    int32_t rlen = 0;
    uint8_t *rxbuf;
    const struct UsartCtrl_t *pCntrl;

    if (port < 0 || port > PORT_TOTAL) {
        EPRINTF(("Invalid port. %d Line %d \n", port, __LINE__));
        return 0 ;
    }

    if (size <= 0) return 0;

    if (Rx_BufSize[port] == 0 || Rx_BufStart[port] == 0) {
        EPRINTF(("Not init for the port %d. Line %d \n", port, __LINE__));
        return 0;
    }

    rxbuf = (uint8_t *)Rx_BufStart[port];
    pCntrl = &myUsartCtrl[port];

    WrPtr_Rx = (int32_t)(((uint32_t) Rx_BufSize[port]) - ((uint32_t)((pCntrl->rx_DMA_Stream)->NDTR))) ;
    fullness = WrPtr_Rx - RdPtr_Rx[port];
    if (fullness < 0) fullness += Rx_BufSize[port];

    if (size > fullness)  rlen  = fullness;
    else rlen = size;

    if (RdPtr_Rx[port] + rlen < Rx_BufSize[port]) {
        memcpy(buf, rxbuf + RdPtr_Rx[port], rlen);
        RdPtr_Rx[port] += rlen;
    } else {
        int32_t len1, len2;
        len1 = Rx_BufSize[port] - RdPtr_Rx[port];
        len2 = rlen - len1;
        memcpy(buf, rxbuf + RdPtr_Rx[port], len1);
        if (len2)
            memcpy(buf + len1, rxbuf, len2);
        RdPtr_Rx[port] = len2;
    }
    return rlen;
}


void Usart_Comm_Init(void)
{
    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    General_Port_Init(COMM_PORT, &USART_InitStructure);

    Usart_Tx_dma_init(COMM_PORT, (uint32_t)comm_txbuf, (uint16_t)COMM_TX_SIZE);
    Usart_Rx_dma_init(COMM_PORT, (uint32_t)comm_rxbuf, (uint16_t)COMM_RX_SIZE);


}

int32_t Usart_Comm_GetSize(void)
{
    return Usart_Get_RxSize(COMM_PORT);
}

int32_t Usart_Comm_Read(uint8_t *buf, int32_t size)
{
    return Usart_Read(COMM_PORT, buf, size);
}

void Usart_Comm_ResetRx(void)
{
    Usart_Reset_Rx(COMM_PORT);
}

int8_t Usart_Comm_WaitTx(int32_t timeout)
{
    do {
        if (tx_busy[COMM_PORT] == 0) return 0;
        DelayUS(1);
    } while (--timeout > 0);

    return -1;
}

int8_t Usart_Comm_TxBusy(void)
{
    return tx_busy[COMM_PORT];
}

void Usart_Comm_Write(uint8_t *buf, int32_t len)
{
    memcpy(comm_txbuf, buf, len);
    Usart_TxFunc[COMM_PORT](len);
    return;
}




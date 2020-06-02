
#include "stm32f4xx.h"
#include "global.h"
#include "i2c_drv.h"
#include "hal.h"

#define mI2C                          I2C1
#define mI2C_CLK                      RCC_APB1Periph_I2C1

#define mI2C_SCL_PIN                  GPIO_Pin_6
#define mI2C_SCL_GPIO_PORT            GPIOB
#define mI2C_SCL_GPIO_CLK             RCC_AHB1Periph_GPIOB
#define mI2C_SCL_SOURCE               GPIO_PinSource6
#define mI2C_SCL_AF                   GPIO_AF_I2C1

#define mI2C_SDA_PIN                  GPIO_Pin_7
#define mI2C_SDA_GPIO_PORT            GPIOB
#define mI2C_SDA_GPIO_CLK             RCC_AHB1Periph_GPIOB
#define mI2C_SDA_SOURCE               GPIO_PinSource7
#define mI2C_SDA_AF                   GPIO_AF_I2C1

#define mI2C_DMA                      DMA1
#define mI2C_DMA_CHANNEL              DMA_Channel_1
#define mI2C_DMA_STREAM_TX            DMA1_Stream7
#define mI2C_DMA_STREAM_RX            DMA1_Stream0
#define mI2C_DMA_CLK                  RCC_AHB1Periph_DMA1
#define mI2C_DR_Address               ((uint32_t)0x40005410)

#define mI2C_DMA_TX_IRQn              DMA1_Stream7_IRQn
#define mI2C_DMA_RX_IRQn              DMA1_Stream0_IRQn
#define mI2C_DMA_TX_IRQHandler        DMA1_Stream7_IRQHandler
#define mI2C_DMA_RX_IRQHandler        DMA1_Stream0_IRQHandler
#define mI2C_DMA_PREPRIO              0
#define mI2C_DMA_SUBPRIO              0

#define mI2C_TX_DMA_FLAG_FEIF             DMA_FLAG_FEIF7
#define mI2C_TX_DMA_FLAG_DMEIF            DMA_FLAG_DMEIF7
#define mI2C_TX_DMA_FLAG_TEIF             DMA_FLAG_TEIF7
#define mI2C_TX_DMA_FLAG_HTIF             DMA_FLAG_HTIF7
#define mI2C_TX_DMA_FLAG_TCIF             DMA_FLAG_TCIF7

#define mI2C_RX_DMA_FLAG_FEIF             DMA_FLAG_FEIF0
#define mI2C_RX_DMA_FLAG_DMEIF            DMA_FLAG_DMEIF0
#define mI2C_RX_DMA_FLAG_TEIF             DMA_FLAG_TEIF0
#define mI2C_RX_DMA_FLAG_HTIF             DMA_FLAG_HTIF0
#define mI2C_RX_DMA_FLAG_TCIF             DMA_FLAG_TCIF0

#define mI2C_DIRECTION_TX                 0
#define mI2C_DIRECTION_RX                 1

#define mI2C_FLAG_TIMEOUT     0x2000
#define mI2C_LONG_TIMEOUT     (30*mI2C_FLAG_TIMEOUT)
#define mI2C_SPEED            (400000)

#define MAX_TRIALS_NUMBER     1000

static volatile int8_t i2c_txbusy = 0;
static volatile int8_t i2c_rxbusy = 0;
static volatile int32_t i2c_error = 0;

static int32_t I2C_rxTimeout_Callback(const char *, int32_t);
static int32_t I2C_txTimeout_Callback(const char *, int32_t);

static DMA_InitTypeDef   sEEDMA_InitStructure = {  0 };
void mI2C_LowLevel_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(mI2C_CLK, ENABLE);
    RCC_AHB1PeriphClockCmd(mI2C_SCL_GPIO_CLK | mI2C_SDA_GPIO_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    RCC_APB1PeriphResetCmd(mI2C_CLK, ENABLE);
    RCC_APB1PeriphResetCmd(mI2C_CLK, DISABLE);

    GPIO_PinAFConfig(mI2C_SCL_GPIO_PORT, mI2C_SCL_SOURCE, mI2C_SCL_AF);
    GPIO_PinAFConfig(mI2C_SDA_GPIO_PORT, mI2C_SDA_SOURCE, mI2C_SDA_AF);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    GPIO_InitStructure.GPIO_Pin = mI2C_SCL_PIN;
    GPIO_Init(mI2C_SCL_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = mI2C_SDA_PIN;
    GPIO_Init(mI2C_SDA_GPIO_PORT, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = mI2C_DMA_TX_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = mI2C_DMA_PREPRIO;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = mI2C_DMA_SUBPRIO;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = mI2C_DMA_RX_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = mI2C_DMA_PREPRIO;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = mI2C_DMA_SUBPRIO;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    RCC_AHB1PeriphClockCmd(mI2C_DMA_CLK, ENABLE);

    DMA_ClearFlag(mI2C_DMA_STREAM_TX, mI2C_TX_DMA_FLAG_FEIF | \
                  mI2C_TX_DMA_FLAG_DMEIF | mI2C_TX_DMA_FLAG_TEIF | \
                  mI2C_TX_DMA_FLAG_HTIF | mI2C_TX_DMA_FLAG_TCIF);

    DMA_Cmd(mI2C_DMA_STREAM_TX, DISABLE);
    DMA_DeInit(mI2C_DMA_STREAM_TX);
    sEEDMA_InitStructure.DMA_Channel = mI2C_DMA_CHANNEL;
    sEEDMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)mI2C_DR_Address;
    sEEDMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)0;
    sEEDMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    sEEDMA_InitStructure.DMA_BufferSize = 0xFFFF;
    sEEDMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    sEEDMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    sEEDMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    sEEDMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    sEEDMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    sEEDMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    sEEDMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
    sEEDMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    sEEDMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    sEEDMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(mI2C_DMA_STREAM_TX, &sEEDMA_InitStructure);

    DMA_ClearFlag(mI2C_DMA_STREAM_RX, mI2C_RX_DMA_FLAG_FEIF | \
                  mI2C_RX_DMA_FLAG_DMEIF | mI2C_RX_DMA_FLAG_TEIF | \
                  mI2C_RX_DMA_FLAG_HTIF | mI2C_RX_DMA_FLAG_TCIF);

    DMA_Cmd(mI2C_DMA_STREAM_RX, DISABLE);
    DMA_DeInit(mI2C_DMA_STREAM_RX);
    DMA_Init(mI2C_DMA_STREAM_RX, &sEEDMA_InitStructure);

    DMA_ITConfig(mI2C_DMA_STREAM_TX, DMA_IT_TC, ENABLE);
    DMA_ITConfig(mI2C_DMA_STREAM_RX, DMA_IT_TC, ENABLE);
}

void mI2C_LowLevel_DeInit(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    I2C_Cmd(mI2C, DISABLE);
    I2C_DeInit(mI2C);

    RCC_APB1PeriphClockCmd(mI2C_CLK, DISABLE);

    GPIO_InitStructure.GPIO_Pin = mI2C_SCL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(mI2C_SCL_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = mI2C_SDA_PIN;
    GPIO_Init(mI2C_SDA_GPIO_PORT, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = mI2C_DMA_TX_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = mI2C_DMA_PREPRIO;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = mI2C_DMA_SUBPRIO;
    NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = mI2C_DMA_RX_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = mI2C_DMA_PREPRIO;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = mI2C_DMA_SUBPRIO;
    NVIC_Init(&NVIC_InitStructure);

    I2C_SoftwareResetCmd(I2C1, ENABLE);
    DelayUS(5);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    DMA_Cmd(mI2C_DMA_STREAM_TX, DISABLE);
    DMA_Cmd(mI2C_DMA_STREAM_RX, DISABLE);
    DMA_DeInit(mI2C_DMA_STREAM_TX);
    DMA_DeInit(mI2C_DMA_STREAM_RX);
    return ;
}

void I2C_SetSlave(uint16_t addr, uint32_t speed)
{
    I2C_InitTypeDef  I2C_InitStructure;

    I2C_DeInit(mI2C);

    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = addr;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = speed;

    I2C_Init(mI2C, &I2C_InitStructure);

    I2C_AcknowledgeConfig(mI2C, ENABLE);
    I2C_Cmd(mI2C, ENABLE);
    return ;
}

void mI2C_Init(void)
{
    mI2C_LowLevel_Init();
    I2C_SetSlave(0, mI2C_SPEED);
    i2c_error  = 0;
}

void mI2C_Reset(void)
{
    mI2C_LowLevel_DeInit();
    mI2C_Init();
}

static void I2C_LowLevel_DMAConfig(uint32_t pBuffer, uint32_t BufferSize, uint32_t Direction)
{
    if (Direction == mI2C_DIRECTION_TX) {
        sEEDMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)pBuffer;
        sEEDMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
        sEEDMA_InitStructure.DMA_BufferSize = (uint32_t)BufferSize;
        DMA_Init(mI2C_DMA_STREAM_TX, &sEEDMA_InitStructure);

        DMA_ClearFlag(mI2C_DMA_STREAM_TX, mI2C_TX_DMA_FLAG_FEIF | \
                      mI2C_TX_DMA_FLAG_DMEIF | mI2C_TX_DMA_FLAG_TEIF | \
                      mI2C_TX_DMA_FLAG_HTIF | mI2C_TX_DMA_FLAG_TCIF);
    } else {
        sEEDMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)pBuffer;
        sEEDMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
        sEEDMA_InitStructure.DMA_BufferSize = (uint32_t)BufferSize;
        DMA_Init(mI2C_DMA_STREAM_RX, &sEEDMA_InitStructure);
        DMA_ClearFlag(mI2C_DMA_STREAM_RX, mI2C_RX_DMA_FLAG_FEIF | \
                      mI2C_RX_DMA_FLAG_DMEIF | mI2C_RX_DMA_FLAG_TEIF | \
                      mI2C_RX_DMA_FLAG_HTIF | mI2C_RX_DMA_FLAG_TCIF);
    }
}


void mI2C_DMA_RX_IRQHandler(void)
{
    if (DMA_GetFlagStatus(mI2C_DMA_STREAM_RX, mI2C_RX_DMA_FLAG_TCIF) != RESET) {
        I2C_GenerateSTOP(mI2C, ENABLE);
        DMA_Cmd(mI2C_DMA_STREAM_RX, DISABLE);
        DMA_ClearFlag(mI2C_DMA_STREAM_RX, mI2C_RX_DMA_FLAG_TCIF);
        i2c_rxbusy = 0;
    }
}

int8_t mI2C_rxBusy(void)
{
    return i2c_rxbusy;
}

int8_t mI2C_txBusy(void)
{
    return i2c_txbusy;
}

int8_t mI2C_Busy(void)
{
    return (i2c_rxbusy || i2c_txbusy);
}

void mI2C_DMA_TX_IRQHandler(void)
{
    volatile uint32_t Timeout_isr  = 0;
    if (DMA_GetFlagStatus(mI2C_DMA_STREAM_TX, mI2C_TX_DMA_FLAG_TCIF) != RESET) {
        DMA_Cmd(mI2C_DMA_STREAM_TX, DISABLE);
        DMA_ClearFlag(mI2C_DMA_STREAM_TX, mI2C_TX_DMA_FLAG_TCIF);

        Timeout_isr = mI2C_LONG_TIMEOUT;
        while (!I2C_GetFlagStatus(mI2C, I2C_FLAG_BTF)) {
            if ((Timeout_isr--) == 0) {
                I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
                break;
            }
        }

        I2C_GenerateSTOP(mI2C, ENABLE);
        i2c_txbusy = 0;
    }
}

int32_t mI2C_WriteBuffer(uint8_t i2c_addr, uint8_t *pBuffer, uint16_t WriteAddr, uint32_t NumByteToWrite, uint32_t flags)
{
    volatile uint32_t Timeout  = 0;

    i2c_txbusy  = 1;

    Timeout = mI2C_LONG_TIMEOUT;
    while (I2C_GetFlagStatus(mI2C, I2C_FLAG_BUSY)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    I2C_GenerateSTART(mI2C, ENABLE);

    Timeout = mI2C_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_MODE_SELECT)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    Timeout = mI2C_FLAG_TIMEOUT;
    I2C_Send7bitAddress(mI2C, i2c_addr , I2C_Direction_Transmitter);

    Timeout = mI2C_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    if ((flags & I2CFLAG_NO_ADDR) == 0) {
        if (flags & I2CFLAG_2BYTES_ADDR) {
            I2C_SendData(mI2C, (uint8_t)((WriteAddr & 0xFF00) >> 8));
            Timeout = mI2C_FLAG_TIMEOUT;
            while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
                if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
            }
        }

        I2C_SendData(mI2C, (uint8_t)(WriteAddr & 0x00FF));
        Timeout = mI2C_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
            if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
        }
    }

    I2C_LowLevel_DMAConfig((uint32_t)pBuffer, NumByteToWrite, mI2C_DIRECTION_TX);

    DMA_Cmd(mI2C_DMA_STREAM_TX, ENABLE);

    I2C_DMACmd(mI2C, ENABLE);

    return 0;
}


int32_t mI2C_WriteByte(uint8_t i2c_addr, uint8_t data, uint16_t WriteAddr, uint32_t flags)
{
    volatile uint32_t Timeout  = 0;

    i2c_txbusy = 1;
    Timeout = mI2C_LONG_TIMEOUT;
    while (I2C_GetFlagStatus(mI2C, I2C_FLAG_BUSY)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    I2C_GenerateSTART(mI2C, ENABLE);
    Timeout = mI2C_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_MODE_SELECT)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    Timeout = mI2C_FLAG_TIMEOUT;
    I2C_Send7bitAddress(mI2C, i2c_addr, I2C_Direction_Transmitter);

    Timeout = mI2C_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    if ((flags & I2CFLAG_NO_ADDR) == 0) {
        if (flags & I2CFLAG_2BYTES_ADDR) {
            I2C_SendData(mI2C, (uint8_t)((WriteAddr & 0xFF00) >> 8));
            Timeout = mI2C_FLAG_TIMEOUT;
            while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
                if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
            }
        }
        I2C_SendData(mI2C, (uint8_t)(WriteAddr & 0x00FF));

        Timeout = mI2C_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
            if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
        }
    }

    I2C_SendData(mI2C, data);

    Timeout = mI2C_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    Timeout = mI2C_LONG_TIMEOUT;
    while (!I2C_GetFlagStatus(mI2C, I2C_FLAG_BTF)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    I2C_GenerateSTOP(mI2C, ENABLE);

    i2c_txbusy = 0;
    return 0;
}

static int32_t I2C_rxTimeout_Callback(const char *file, int32_t line)
{
    i2c_error  ++;
    EPRINTF(("I2C Rx timeout %d @%s - %d\n", i2c_error, file, line));
    i2c_rxbusy = 0;
    return -1;
}

static int32_t I2C_txTimeout_Callback(const char *file, int32_t line)
{
    i2c_error  ++;
    EPRINTF(("I2C Tx timeout %d @%s - %d\n", i2c_error, file, line));
    i2c_txbusy = 0;
    return -1;
}

int32_t mI2C_BusError(void)
{
    return (i2c_error) ? 1 : 0;
}

int32_t mI2C_ReadBuffer(uint8_t i2c_addr, uint8_t *pBuffer, uint16_t ReadAddr, uint32_t NumByteToRead, uint32_t flags)
{
    volatile int32_t Timeout  = 0;
    i2c_rxbusy = 1;

    Timeout = mI2C_LONG_TIMEOUT;
    while (I2C_GetFlagStatus(mI2C, I2C_FLAG_BUSY)) {
        if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
    }

    if ((flags & I2CFLAG_NO_ADDR) == 0) {
        I2C_GenerateSTART(mI2C, ENABLE);

        Timeout = mI2C_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_MODE_SELECT)) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }

        I2C_Send7bitAddress(mI2C, i2c_addr, I2C_Direction_Transmitter);
        Timeout = mI2C_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }

        if (flags & I2CFLAG_2BYTES_ADDR) {
            I2C_SendData(mI2C, (uint8_t)((ReadAddr & 0xFF00) >> 8));
            Timeout = mI2C_FLAG_TIMEOUT;
            while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
                if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
            }
        }
        I2C_SendData(mI2C, (uint8_t)(ReadAddr & 0x00FF));

        Timeout = mI2C_FLAG_TIMEOUT;
        while (I2C_GetFlagStatus(mI2C, I2C_FLAG_BTF) == RESET) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }
    }

    I2C_GenerateSTART(mI2C, ENABLE);

    Timeout = mI2C_FLAG_TIMEOUT;
    while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_MODE_SELECT)) {
        if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
    }

    I2C_Send7bitAddress(mI2C, i2c_addr , I2C_Direction_Receiver);

    if (NumByteToRead < 2) {
        Timeout = mI2C_FLAG_TIMEOUT;
        while (I2C_GetFlagStatus(mI2C, I2C_FLAG_ADDR) == RESET) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }

        I2C_AcknowledgeConfig(mI2C, DISABLE);
        /* Clear ADDR register by reading SR1 then SR2 register (SR1 has already been read) */
        (void)mI2C->SR2;
        I2C_GenerateSTOP(mI2C, ENABLE);
        Timeout = mI2C_FLAG_TIMEOUT;
        while (I2C_GetFlagStatus(mI2C, I2C_FLAG_RXNE) == RESET) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }
        *pBuffer = I2C_ReceiveData(mI2C);
        NumByteToRead--;
        pBuffer++;

        Timeout = mI2C_FLAG_TIMEOUT;
        while (mI2C->CR1 & I2C_CR1_STOP) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }
        I2C_AcknowledgeConfig(mI2C, ENABLE);
        i2c_rxbusy = 0;
    } else {
        Timeout = mI2C_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) {
            if ((Timeout--) == 0) return I2C_rxTimeout_Callback(__FUNCTION__, __LINE__);
        }

        I2C_LowLevel_DMAConfig((uint32_t)pBuffer, NumByteToRead, mI2C_DIRECTION_RX);

        I2C_DMALastTransferCmd(mI2C, ENABLE);
        DMA_Cmd(mI2C_DMA_STREAM_RX, ENABLE);
        I2C_DMACmd(mI2C, ENABLE);
    }

    return 0;
}

void mI2C_ErrorHandler(void)
{
    if (i2c_error) {
        mI2C_Reset();
        i2c_error  = 0;
    }
}

int32_t mI2C_WaitStandbyState(uint8_t addr)
{
    volatile int32_t Timeout  = 0;
    volatile uint16_t tmpSR1 = 0;
    volatile uint32_t sEETrials = 0;
    i2c_txbusy = 1;
    Timeout = mI2C_LONG_TIMEOUT;
    while (I2C_GetFlagStatus(mI2C, I2C_FLAG_BUSY)) {
        if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
    }

    while (1) {
        I2C_GenerateSTART(mI2C, ENABLE);
        Timeout = mI2C_FLAG_TIMEOUT;
        while (!I2C_CheckEvent(mI2C, I2C_EVENT_MASTER_MODE_SELECT)) {
            if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
        }
        I2C_Send7bitAddress(mI2C, addr, I2C_Direction_Transmitter);

        Timeout = mI2C_LONG_TIMEOUT;
        do {
            tmpSR1 = mI2C->SR1;
            if ((Timeout--) == 0) return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
        } while ((tmpSR1 & (I2C_SR1_ADDR | I2C_SR1_AF)) == 0);

        if (tmpSR1 & I2C_SR1_ADDR) {
            (void)mI2C->SR2;
            I2C_GenerateSTOP(mI2C, ENABLE);
            i2c_txbusy = 0;
            return 0;
        } else {
            I2C_ClearFlag(mI2C, I2C_FLAG_AF);
        }

        if (sEETrials++ == MAX_TRIALS_NUMBER) {
            return I2C_txTimeout_Callback(__FUNCTION__, __LINE__);
        }
    }
}



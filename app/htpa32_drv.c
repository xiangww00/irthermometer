

#include "global.h"
#include "hal.h"
#include "i2c_drv.h"
#include "htpa32_drv.h"

#define HTPA32_CHIP_ADRR  (0x1A << 1)
#define HTPA32_EEPR_ADDR  (0x50 << 1)

#define sEE_PAGESIZE  (32)
#define EEP_TIMEOUT  (50*0x2000)
#define BUS_TIMEOUT  (50000)  //us

static inline int32_t i2c_CheckTimeout(uint32_t timeout)
{
    if (mI2C_Busy() == 0) return 0;

    do {
        if (mI2C_Busy() == 0) {
            return 0;
        }
        DelayUS(1);
    } while (timeout--);

    return -1;
}

void Htpa32_Wait2Idle(void)
{
    i2c_CheckTimeout(BUS_TIMEOUT * 10);
}

void Htpa32_Drv_Init(void)
{
    Sensor_Ctrl_Init();
    Sensor_Ctrl_on();
    DelayUS(100);
    mI2C_Init();
    DelayUS(100);

    return ;
}

int32_t Htpa32_WriteReg(uint8_t reg, uint8_t data)
{
    int32_t result;

    if (i2c_CheckTimeout(BUS_TIMEOUT)) {
        EPRINTF(("I2C bus busy. %d \n", __LINE__));
        return -1;
    }

    result = mI2C_WriteByte(HTPA32_CHIP_ADRR, data, reg, 0);

    return result;
}

uint8_t Htpa32_GetStatus(void)
{
    uint8_t status = 0;
    Htpa32_ReadReg(REG_STATUS, &status);
    return status;
}

int32_t Htpa32_ReadReg(uint8_t reg, uint8_t *data)
{
    int32_t result;

    if (i2c_CheckTimeout(BUS_TIMEOUT)) {
        EPRINTF(("I2C bus busy. %d \n", __LINE__));
        return -1;
    }

    result  = mI2C_ReadBuffer(HTPA32_CHIP_ADRR, data, reg, 1, 0);
    return result;
}

int32_t Htpa32_ReadData(uint8_t reg, uint8_t *buf, uint32_t size)
{
    int32_t result;

    if (i2c_CheckTimeout(BUS_TIMEOUT)) {
        EPRINTF(("I2C bus busy. %d \n", __LINE__));
        return -1;
    }

    result = mI2C_ReadBuffer(HTPA32_CHIP_ADRR, buf, reg, size, 0);
    return result;
}

int8_t Htpa32_isIdle(void)
{
    return (mI2C_Busy() ? 0 : 1);
}


int32_t Htpa32_EEPROM_Read(uint16_t addr, uint8_t *buf, uint32_t size)
{
    int32_t result;

    if (i2c_CheckTimeout(BUS_TIMEOUT)) {
        EPRINTF(("I2C bus busy. %d \n", __LINE__));
        return -1;
    }

    result = mI2C_ReadBuffer(HTPA32_EEPR_ADDR, buf, addr, size, I2CFLAG_2BYTES_ADDR);
    return result;
}

int32_t Htpa32_EEPROM_ReadDone(uint16_t addr, uint8_t *buf, uint32_t size)
{
    int32_t result;
    result = Htpa32_EEPROM_Read(addr, buf, size);
    if (result) return result;
    Htpa32_Wait2Idle();
    return 0;

}

int32_t Htpa32_EEPROM_ReadByte(uint16_t Addr, uint8_t *data)
{
    int32_t result;
    result = Htpa32_EEPROM_Read(Addr, data, 1);
    if (result) return result;
    Htpa32_Wait2Idle();
    return 0;
}

int32_t Htpa32_EEPROM_ReadWord(uint16_t Addr, uint16_t *data)
{
    int32_t result;
    result = Htpa32_EEPROM_Read(Addr, (uint8_t *)data, 2);
    if (result) return result;
    Htpa32_Wait2Idle();
    return 0;
}


int32_t Htpa32_EEPROM_WriteByte(uint16_t WriteAddr, uint8_t data)
{
    if (i2c_CheckTimeout(BUS_TIMEOUT)) {
        EPRINTF(("I2C bus busy. %d \n", __LINE__));
        return -1;
    }
    mI2C_WriteByte(HTPA32_EEPR_ADDR, data, WriteAddr, I2CFLAG_2BYTES_ADDR);
    mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
    return 0;
}

int32_t Htpa32_EEPROM_WriteWord(uint16_t WriteAddr, uint16_t data)
{
    int32_t result;
    if (i2c_CheckTimeout(BUS_TIMEOUT)) {
        EPRINTF(("I2C bus busy. %d \n", __LINE__));
        return -1;
    }
    result = Htpa32_EEPROM_WriteByte(WriteAddr, (uint8_t)(data & 0x00ff));
    if (result) return result;
    result = Htpa32_EEPROM_WriteByte(WriteAddr + 1, (uint8_t)((data >> 8) & 0x00ff));
    return result;

}

int32_t Htpa32_EEPROM_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint32_t size)
{

    volatile uint32_t sEETimeout   = 0;
    uint8_t NumOfPage = 0, NumOfSingle = 0, count = 0;
    uint16_t Addr = 0;
    uint32_t WriteSize;

    Addr = WriteAddr % sEE_PAGESIZE;
    count = sEE_PAGESIZE - Addr;
    NumOfPage =  size / sEE_PAGESIZE;
    NumOfSingle = size % sEE_PAGESIZE;

    if (Addr == 0) {
        if (NumOfPage == 0) {
            WriteSize = NumOfSingle;
            mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

            sEETimeout = EEP_TIMEOUT;
            while (mI2C_txBusy()) {
                if ((sEETimeout--) == 0) {
                    EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                    return -1;
                }
            }
            mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
        } else {
            while (NumOfPage--) {
                WriteSize = sEE_PAGESIZE;
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);

                WriteAddr +=  sEE_PAGESIZE;
                pBuffer += sEE_PAGESIZE;
            }

            if (NumOfSingle != 0) {
                WriteSize = NumOfSingle;
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
            }
        }
    } else {
        if (NumOfPage == 0) {

            if (size > count) {
                WriteSize = count;

                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);

                WriteSize = (size - count);
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, (uint8_t *)(pBuffer + count), (WriteAddr + count), WriteSize , I2CFLAG_2BYTES_ADDR);
                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
            } else {
                WriteSize = NumOfSingle;
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
            }
        } else {
            size -= count;
            NumOfPage =  size / sEE_PAGESIZE;
            NumOfSingle = size % sEE_PAGESIZE;

            if (count != 0) {
                WriteSize = count;
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
                WriteAddr += count;
                pBuffer += count;
            }

            while (NumOfPage--) {
                WriteSize = sEE_PAGESIZE;
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);

                WriteAddr +=  sEE_PAGESIZE;
                pBuffer += sEE_PAGESIZE;
            }
            if (NumOfSingle != 0) {
                WriteSize = NumOfSingle;
                mI2C_WriteBuffer(HTPA32_EEPR_ADDR, pBuffer, WriteAddr, WriteSize , I2CFLAG_2BYTES_ADDR);

                sEETimeout = EEP_TIMEOUT;
                while (mI2C_txBusy()) {
                    if ((sEETimeout--) == 0) {
                        EPRINTF(("Write EEPROM failed. %s %d \n", __FUNCTION__, __LINE__));
                        return -1;
                    }
                }
                mI2C_WaitStandbyState(HTPA32_EEPR_ADDR);
            }
        }
    }
    return 0;
}



#ifndef __I2C_DRV__
#define __I2C_DRV__

#define I2CFLAG_2BYTES_ADDR  (0x00000001)
#define I2CFLAG_NO_ADDR      (0x00000002)

void mI2C_Init(void);
void mI2C_Reset(void);
int32_t mI2C_WriteBuffer(uint8_t i2c_addr, uint8_t *pBuffer, uint16_t WriteAddr, uint32_t NumByteToWrite, uint32_t flags);
int32_t mI2C_WriteByte(uint8_t i2c_addr, uint8_t data, uint16_t WriteAddr, uint32_t flags);
int32_t mI2C_ReadBuffer(uint8_t i2c_addr, uint8_t *pBuffer, uint16_t ReadAddr, uint32_t NumByteToRead, uint32_t flags);

int32_t mI2C_WaitStandbyState(uint8_t addr);

int8_t mI2C_rxBusy(void);
int8_t mI2C_txBusy(void);
int8_t mI2C_Busy(void);

int32_t mI2C_BusError(void);

#endif


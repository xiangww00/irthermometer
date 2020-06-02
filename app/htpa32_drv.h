
#ifndef __HTPA32_DRV_H__
#define __HTPA32_DRV_H__

#define REG_CONFIG    0x01
#define REG_STATUS    0x02
#define REG_MBIT      0x03
#define REG_BIASL     0x04
#define REG_BIASR     0x05
#define REG_CLK       0x06
#define REG_BPAL      0x07
#define REG_BPAR      0x08
#define REG_PU        0x09

#define REG_READ1     0x0a
#define REG_READ2     0x0b

void Htpa32_Drv_Init(void);

int32_t Htpa32_WriteReg(uint8_t reg, uint8_t data);
int32_t Htpa32_ReadReg(uint8_t reg, uint8_t *data);
int32_t Htpa32_ReadData(uint8_t reg, uint8_t *buf, uint32_t size);
int8_t Htpa32_isIdle(void);
void Htpa32_Wait2Idle(void);
uint8_t Htpa32_GetStatus(void);

int32_t Htpa32_EEPROM_Read(uint16_t addr, uint8_t *buf, uint32_t size);
int32_t Htpa32_EEPROM_ReadDone(uint16_t addr, uint8_t *buf, uint32_t size);
int32_t Htpa32_EEPROM_WriteByte(uint16_t WriteAddr, uint8_t data);
int32_t Htpa32_EEPROM_ReadByte(uint16_t Addr, uint8_t *data);
int32_t Htpa32_EEPROM_ReadWord(uint16_t Addr, uint16_t *data);
int32_t Htpa32_EEPROM_WriteWord(uint16_t WriteAddr, uint16_t data);
int32_t Htpa32_EEPROM_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint32_t size);

#endif



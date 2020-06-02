

#include "common.h"
#include "flash_if.h"
#include "hal.h"

static uint32_t GetSector(uint32_t Address);


void FLASH_If_Init(void)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

void FLASH_If_Done(void)
{
    FLASH_Lock();
}

uint32_t FLASH_If_EraseUser(void)
{
    uint32_t UserStartSector , i = 0;
    UserStartSector = GetSector(USER_START_ADDRESS);
    for (i = UserStartSector; i <= FLASH_Sector_11; i += 8) {
        if (FLASH_EraseSector(i, VoltageRange_3) != FLASH_COMPLETE) {
            return (1);
        }
    }
    return (0);
}

uint32_t FLASH_If_EraseData(void)
{
    uint32_t ret = 0;
    uint32_t UserStartSector;
    int8_t retry = 3;
    int8_t err = 0;
    UserStartSector = GetSector(USER_START_ADDRESS);

    FLASH_If_Init();
    err = -1;
    do {
        if (FLASH_EraseSector(UserStartSector , VoltageRange_3) == FLASH_COMPLETE) {
            err = 0;
            break;
        }
        DelayUS(10);
    } while (--retry > 0);

    if (err) ret = 1;
    FLASH_Lock();
    return ret ;
}

uint32_t FLASH_If_EraseApp(void)
{
    uint32_t ret = 0;
    uint32_t UserStartSector , i = 0;

    UserStartSector = GetSector(APP_START_ADDRESS);

    FLASH_If_Init();
    for (i = UserStartSector; i <= FLASH_Sector_11; i += 8) {
        if (FLASH_EraseSector(i, VoltageRange_3) != FLASH_COMPLETE) {
            ret = 1;
            break;
        }
    }
    FLASH_Lock();
    return ret;
}

uint32_t FLASH_If_EraseAppBySize(uint16_t size)
{
    uint32_t ret = 0;
    uint32_t UserStartSector , i = 0;
    uint32_t UserEndSector;
    int8_t retry = 3;
    int8_t err = 0;

    UserStartSector = GetSector(APP_START_ADDRESS);
    UserEndSector = GetSector(APP_START_ADDRESS + size);

    if (UserEndSector > FLASH_Sector_11) return 2;

    FLASH_If_Init();
    for (i = UserStartSector; i <= UserEndSector; i += 8) {
        err = -1;
        do {
            if (FLASH_EraseSector(i, VoltageRange_3) == FLASH_COMPLETE) {
                err = 0;
                break;
            }
        } while (-- retry > 0);
        if (err) {
            ret = 1;
            break;
        }
    }
    FLASH_Lock();
    return ret;

}

uint32_t FLASH_If_EraseSector(uint32_t StartSector, uint32_t Sectors)
{
    uint32_t ret = 0;
    uint32_t FlashSector = StartSector * 8;

    FLASH_If_Init();
    for (int i = 0; i < (int)Sectors; i++) {
        if (FLASH_EraseSector(FlashSector, VoltageRange_3) != FLASH_COMPLETE) {
            ret = 1;
            break;
        }
        FlashSector += 8;
    }
    FLASH_Lock();
    return ret;
}

uint32_t FLASH_If_Write(uint32_t FlashAddress, uint32_t *Data , uint16_t DataLength)
{
    uint32_t ret = 0;
    uint32_t i = 0;
    FLASH_If_Init();
    for (i = 0; (i < DataLength) && (FlashAddress <= (USER_FLASH_END_ADDRESS - 4)); i++) {
        if (FLASH_ProgramWord(FlashAddress, *(uint32_t *)(Data + i)) == FLASH_COMPLETE) {
            if (*(uint32_t *)FlashAddress != *(uint32_t *)(Data + i)) {
                ret = 2;
                break;
            }
            FlashAddress += 4;
        } else {
            ret = 1;
            break;
        }
    }

    FLASH_Lock();
    return (ret);
}

uint16_t FLASH_If_GetWriteProtectionStatus(void)
{
    uint32_t UserStartSector = FLASH_Sector_1;

    UserStartSector = GetSector(USER_START_ADDRESS);

    if ((FLASH_OB_GetWRP() >> (UserStartSector / 8)) == (0xFFF >> (UserStartSector / 8))) {
        return 1;
    } else {
        return 0;
    }
}

uint32_t FLASH_If_DisableWriteProtection(void)
{
    uint32_t UserStartSector = FLASH_Sector_1, UserWrpSectors = OB_WRP_Sector_1;

    UserStartSector = GetSector(USER_START_ADDRESS);

    UserWrpSectors = 0xFFF - ((1 << (UserStartSector / 8)) - 1);

    FLASH_OB_Unlock();

    FLASH_OB_WRPConfig(UserWrpSectors, DISABLE);

    if (FLASH_OB_Launch() != FLASH_COMPLETE) {
        return (2);
    }

    return (1);
}

/**
 * @brief  Gets the sector of a given address
 * @param  Address: Flash address
 * @retval The sector of a given address
 */
static uint32_t GetSector(uint32_t Address)
{
    uint32_t sector = 0;

    if ((Address < ADDR_FLASH_SECTOR_1) && (Address >= ADDR_FLASH_SECTOR_0)) {
        sector = FLASH_Sector_0;
    } else if ((Address < ADDR_FLASH_SECTOR_2) && (Address >= ADDR_FLASH_SECTOR_1)) {
        sector = FLASH_Sector_1;
    } else if ((Address < ADDR_FLASH_SECTOR_3) && (Address >= ADDR_FLASH_SECTOR_2)) {
        sector = FLASH_Sector_2;
    } else if ((Address < ADDR_FLASH_SECTOR_4) && (Address >= ADDR_FLASH_SECTOR_3)) {
        sector = FLASH_Sector_3;
    } else if ((Address < ADDR_FLASH_SECTOR_5) && (Address >= ADDR_FLASH_SECTOR_4)) {
        sector = FLASH_Sector_4;
    } else if ((Address < ADDR_FLASH_SECTOR_6) && (Address >= ADDR_FLASH_SECTOR_5)) {
        sector = FLASH_Sector_5;
    } else if ((Address < ADDR_FLASH_SECTOR_7) && (Address >= ADDR_FLASH_SECTOR_6)) {
        sector = FLASH_Sector_6;
    } else if ((Address < ADDR_FLASH_SECTOR_8) && (Address >= ADDR_FLASH_SECTOR_7)) {
        sector = FLASH_Sector_7;
    } else if ((Address < ADDR_FLASH_SECTOR_9) && (Address >= ADDR_FLASH_SECTOR_8)) {
        sector = FLASH_Sector_8;
    } else if ((Address < ADDR_FLASH_SECTOR_10) && (Address >= ADDR_FLASH_SECTOR_9)) {
        sector = FLASH_Sector_9;
    } else if ((Address < ADDR_FLASH_SECTOR_11) && (Address >= ADDR_FLASH_SECTOR_10)) {
        sector = FLASH_Sector_10;
    } else if ((Address < ADDR_FLASH_SECTOR_12) && (Address >= ADDR_FLASH_SECTOR_11)) {
        sector = FLASH_Sector_11;
    } else if ((Address < ADDR_FLASH_SECTOR_13) && (Address >= ADDR_FLASH_SECTOR_12)) {
        sector = FLASH_Sector_12;
    } else if ((Address < ADDR_FLASH_SECTOR_14) && (Address >= ADDR_FLASH_SECTOR_13)) {
        sector = FLASH_Sector_13;
    } else if ((Address < ADDR_FLASH_SECTOR_15) && (Address >= ADDR_FLASH_SECTOR_14)) {
        sector = FLASH_Sector_14;
    } else if ((Address < ADDR_FLASH_SECTOR_16) && (Address >= ADDR_FLASH_SECTOR_15)) {
        sector = FLASH_Sector_15;
    } else if ((Address < ADDR_FLASH_SECTOR_17) && (Address >= ADDR_FLASH_SECTOR_16)) {
        sector = FLASH_Sector_16;
    } else if ((Address < ADDR_FLASH_SECTOR_18) && (Address >= ADDR_FLASH_SECTOR_17)) {
        sector = FLASH_Sector_17;
    } else if ((Address < ADDR_FLASH_SECTOR_19) && (Address >= ADDR_FLASH_SECTOR_18)) {
        sector = FLASH_Sector_18;
    } else if ((Address < ADDR_FLASH_SECTOR_20) && (Address >= ADDR_FLASH_SECTOR_19)) {
        sector = FLASH_Sector_19;
    } else if ((Address < ADDR_FLASH_SECTOR_21) && (Address >= ADDR_FLASH_SECTOR_20)) {
        sector = FLASH_Sector_20;
    } else if ((Address < ADDR_FLASH_SECTOR_22) && (Address >= ADDR_FLASH_SECTOR_21)) {
        sector = FLASH_Sector_21;
    } else if ((Address < ADDR_FLASH_SECTOR_23) && (Address >= ADDR_FLASH_SECTOR_22)) {
        sector = FLASH_Sector_22;
    } else { /*(Address < FLASH_END_ADDR) && (Address >= ADDR_FLASH_SECTOR_23))*/
        sector = FLASH_Sector_23;
    }

    return sector;
}





#include <string.h>

#include "hal.h"
#include "global.h"
#include "usart.h"
#include "crc8.h"
#include "comslave.h"
#include "setting.h"
#include "htpa32_main.h"
#include "htpa32_table.h"


#define SYNC_BYTE_CP   0xA5
#define SYNC_BYTE      0x16

#define CMD_SIZE      1552

#define POS_SYNC      0x00
#define POS_CMD       0x01
#define POS_PAYLOAD   0x02

typedef enum {
    RX_SYNC = 0,
    RX_WAIT,
    RX_DATA,
    RX_EXE,
    RX_RESP,
    RX_RST,
} RX_STATE;

typedef struct {
    uint8_t FunCode;
    uint8_t mode;
    eCode(*CmdFuc)(uint8_t *pucFrame, uint16_t len);
} CmdHandler;

static int8_t Rx_State = 0;
static uint8_t Cmd_Buf[CMD_SIZE] __attribute__((aligned(4)));

static uint8_t Get_CheckSum8(uint8_t *buf, int32_t size);
static uint16_t Get_CheckSum16(uint16_t *buf, int32_t size);
static void ComCmdSendCpFrame(uint8_t *pucFrame, uint16_t usLength);
static void ComCmdSendFrame(uint8_t *pucFrame, uint16_t usLength);
static eCode ComCmdGetVersion(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetStatus(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSystemReboot(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGotoBootLoader(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetCenter(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetRectData(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetDeadPixel(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdCpGetObjTemp(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdCpGetAllTemp(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetSample(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetObjTemp(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSetOffset(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetOffset(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdStoreOffset(uint8_t *pucFrame, uint16_t len);


const CmdHandler InstCmdHandler[] = {
    { 0x01, 0x00, ComCmdGetVersion },
    { 0x02, 0x00, ComCmdGetStatus },
    { 0x03, 0x00, ComCmdSystemReboot},
    { 0x04, 0x00, ComCmdGotoBootLoader},
    { 0x10, 0x00, ComCmdGetCenter},
    { 0x11, 0x00, ComCmdGetRectData},
    { 0x12, 0x00, ComCmdGetDeadPixel},
    { 0x13, 0x00, ComCmdGetSample},
    { 0x14, 0x00, ComCmdGetObjTemp},
    { 0x15, 0x00, ComCmdSetOffset},
    { 0x16, 0x00, ComCmdGetOffset},
    { 0x17, 0x00, ComCmdStoreOffset},

    { 0x35, 0x01, ComCmdCpGetAllTemp},
    { 0x55, 0x01, ComCmdCpGetObjTemp},

};

static eCode ComCmdSetOffset(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    int16_t offset;

    if (len != 2) return ECODE_INVALID_PARAM;

    offset = (int16_t)((pucFrame[3] << 8) | pucFrame[2]);
    Htpa32_SetUserOffset(offset);

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}


static eCode ComCmdGetObjTemp(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    uint16_t data;
    struct OutData_t *pSensor;

    if (len != 0) return ECODE_INVALID_PARAM;

    pSensor = Htpa32_GetResult();
    data = (uint16_t)(pSensor->objtemp);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)(pSensor->obj_x);
    pucFrame[usLength++] = (uint8_t)(pSensor->obj_y);
    data = (uint16_t)(pSensor->avgtemp);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
    data = (uint16_t)(pSensor->ta);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);

    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;

}

static eCode ComCmdCpGetObjTemp(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    uint16_t data;
    struct OutData_t *pSensor;

    if (pucFrame[2] != 0x01)  return ECODE_SUCCESS;
    if (len != 1) return ECODE_SUCCESS;

    pSensor = Htpa32_GetResult();
    data = (uint16_t)(pSensor->objtemp);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)(pSensor->obj_x);
    pucFrame[usLength++] = (uint8_t)(pSensor->obj_y);
    ComCmdSendCpFrame(pucFrame, usLength);

    return ECODE_SUCCESS;

}

static eCode ComCmdCpGetAllTemp(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 0;
    struct OutData_t *pSensor;
    uint16_t data;
    uint16_t checksum = 0;
    int32_t ta_c;


    if (pucFrame[2] != 0xF1)  return ECODE_SUCCESS;
    if (len != 1) return ECODE_SUCCESS;
    pSensor = Htpa32_GetResult();

    pucFrame[usLength++] = (uint8_t)0x5A;
    pucFrame[usLength++] = (uint8_t)0x5A; //header 0x5a5a
    pucFrame[usLength++] = (uint8_t)0x06;
    pucFrame[usLength++] = (uint8_t)0x06; //length 0x0606

    data = (uint16_t)(pSensor->objtemp);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)(pSensor->obj_x);
    pucFrame[usLength++] = (uint8_t)(pSensor->obj_y);

    memcpy(&pucFrame[usLength], &(pSensor->out[128]), 768 * 2);
    usLength += (768 * 2);

    ta_c = pSensor->ta;
    ta_c = (ta_c * 10) - 27315;
    data = (uint16_t)(ta_c);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);

    checksum = Get_CheckSum16((uint16_t *) &pucFrame[0], usLength / 2);
    pucFrame[usLength++] = (uint8_t)(checksum & 0xff);
    pucFrame[usLength++] = (uint8_t)((checksum >> 8) & 0xff);

    Usart_Comm_WaitTx(10000);
    Usart_Comm_Write(pucFrame, (int32_t)usLength);

    return ECODE_SUCCESS;
}

static eCode ComCmdStoreOffset(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    int16_t offset;
    int16_t sys;
    if (len != 0) return ECODE_INVALID_PARAM;
    offset = Htpa32_GetUserOffset();
    sys = Sys_GetUsrOffset();
    if (sys != offset) {
        if (Sys_SetUsrOffset(offset) != 0) {
            pucFrame[usLength++] = ECODE_CMD_FAIL;
        } else {
            pucFrame[usLength++] = 0;
        }
    } else {
        pucFrame[usLength++] = 0;
    }
    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetOffset(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    int16_t offset;
    if (len != 0) return ECODE_INVALID_PARAM;
    offset = Htpa32_GetUserOffset();
    pucFrame[usLength++] = 0;
    pucFrame[usLength++] = (uint8_t)(offset & 0xff);
    pucFrame[usLength++] = (uint8_t)((offset >> 8) & 0xff);
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}


static eCode ComCmdGetDeadPixel(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    if (len != 0) return ECODE_INVALID_PARAM;

    pucFrame[usLength++] = 0x00;
    pucFrame[usLength++] = Htpa32_GetDeadp();

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetCenter(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    uint16_t data;
    struct OutData_t *pSensor;
    int i, j;
    pSensor = Htpa32_GetResult();

    data = (uint16_t)(pSensor->ta);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);

    for (i = 14; i < 18; i++) {
        for (j = 14; j < 18; j++) {
            data = (uint16_t)(pSensor->out[i * 32 + j]);
            pucFrame[usLength++] = (uint8_t)(data & 0xff);
            pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
        }
    }
    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetSample(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    uint16_t data;
    struct OutData_t *pSensor;
    uint8_t i, j;
    uint8_t sx, sy, w, h;
    uint8_t ex, ey;
    if (len != 4) return ECODE_INVALID_PARAM;
    sx = pucFrame[2];
    sy = pucFrame[3];
    w = pucFrame[4];
    h = pucFrame[5];

    if ((sx > 31) || (sy > 31)) return ECODE_INVALID_PARAM;

    ex = (sx + w > 32) ? 32 : sx + w;
    ey = (sy + h > 32) ? 32 : sy + h;

    pSensor = Htpa32_GetResult();

    data = (uint16_t)(pSensor->ta);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);

    for (i = sy; i < ey; i++) {
        for (j = sx; j < ex; j++) {
            data = (uint16_t)(pSensor->sample[i * 32 + j]);
            pucFrame[usLength++] = (uint8_t)(data & 0xff);
            pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
        }
    }
    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetRectData(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    uint16_t data;
    struct OutData_t *pSensor;
    uint8_t i, j;
    uint8_t sx, sy, w, h;
    uint8_t ex, ey;
    if (len != 4) return ECODE_INVALID_PARAM;
    sx = pucFrame[2];
    sy = pucFrame[3];
    w = pucFrame[4];
    h = pucFrame[5];

    if ((sx > 31) || (sy > 31)) return ECODE_INVALID_PARAM;

    ex = (sx + w > 32) ? 32 : sx + w;
    ey = (sy + h > 32) ? 32 : sy + h;

    pSensor = Htpa32_GetResult();

    data = (uint16_t)(pSensor->ta);
    pucFrame[usLength++] = (uint8_t)(data & 0xff);
    pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);

    for (i = sy; i < ey; i++) {
        for (j = sx; j < ex; j++) {
            data = (uint16_t)(pSensor->out[i * 32 + j]);
            pucFrame[usLength++] = (uint8_t)(data & 0xff);
            pucFrame[usLength++] = (uint8_t)((data >> 8) & 0xff);
        }
    }
    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static void ComCmdSendFrame(uint8_t *pucFrame, uint16_t usLength)
{
    uint8_t crc;

    crc = Get_CRC8(pucFrame, usLength);
    pucFrame[usLength++] =  crc;
    Usart_Comm_WaitTx(10000);
    Usart_Comm_Write(pucFrame, (int32_t)usLength);
    return ;
}

static void ComCmdSendCpFrame(uint8_t *pucFrame, uint16_t usLength)
{
    uint8_t crc;
    crc = Get_CheckSum8(pucFrame, usLength);
    pucFrame[usLength++] =  crc;
    Usart_Comm_WaitTx(10000);
    Usart_Comm_Write(pucFrame, (int32_t)usLength);
    return ;
}

static eCode ComCmdSystemReboot(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    if (len != 0) return  ECODE_INVALID_PARAM;

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);
    Usart_Comm_WaitTx(10000);

    NVIC_SystemReset();
    return ECODE_SUCCESS;
}

static eCode ComCmdGotoBootLoader(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;
    if (len != 0) return  ECODE_INVALID_PARAM;

    if (Sys_Set2BL()) {
        EPRINTF(("Failed at System Set.\n"));
        return ECODE_EXE_ERROR ;
    }
    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);
    Sensor_Ctrl_off();
    Usart_Comm_WaitTx(10000);

    NVIC_SystemReset();
    return ECODE_SUCCESS;
}

static eCode ComCmdGetVersion(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    int32_t length;
    char *sbuf;
    uint16_t usLength = POS_PAYLOAD;

    sbuf = (char *) &pucFrame[POS_PAYLOAD];
    Sys_GetVer(sbuf);
    length = strlen(sbuf);
    usLength += (length + 1);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetStatus(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = POS_PAYLOAD;

    if (len != 0) return  ECODE_INVALID_PARAM;
    pucFrame[usLength++] = 0;
    pucFrame[usLength++] = 0x10;
    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}


static uint8_t Get_CheckSum8(uint8_t *buf, int32_t size)
{
    uint8_t sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buf[i];
    }
    return sum;
}

static uint16_t Get_CheckSum16(uint16_t *buf, int32_t size)
{
    uint16_t sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buf[i];
    }
    return sum;
}

void ComSlave_Init(void)
{
    Usart_Comm_Init();
    Usart_Comm_ResetRx();
    return ;
}

static void ComEcho_ECode(uint8_t *pucFrame, eCode code)
{
    uint16_t usLength = POS_PAYLOAD;

    pucFrame[usLength++] = code ;
    ComCmdSendFrame(pucFrame, usLength);
    return ;
}

void ComSlave_Service(void)
{
    uint32_t i;
    int32_t fullness;
    int32_t dt, dcnt;

    static eCode ErrorCode;
    static int64_t rx_start = 0;
    static int32_t last_cnt = 0;
    static int32_t Rx_len = 0;
    static uint8_t compatible = 0;

    static uint8_t *pucFrame;
    static uint8_t ucFunctionCode;
    static uint16_t usLength;

    fullness = Usart_Comm_GetSize();

    switch (Rx_State) {
        case RX_SYNC:
            if (!fullness) break;
            rx_start  = Hal_GetTicks();
            last_cnt  = fullness;
            Rx_State = RX_WAIT;
            break;

        case RX_WAIT:
            dt = (int32_t)(Hal_GetTicks() - rx_start);
            dcnt = fullness - last_cnt;

            if ((dt > 50)
                || ((dcnt > 0) && (dt > (dcnt  + 1)))
                || ((dcnt == 0) && (dt > 2))
               ) {
                Rx_State = RX_DATA;
            }
            break;

        case RX_DATA:
            Rx_len = Usart_Comm_Read((uint8_t *) Cmd_Buf, fullness);
            compatible  = 0;

#if 0
            DPRINTF(("Usart Data: "));
            for (int k = 0; k < Rx_len; k++) {
                DPRINTF(("0x%02x ", Cmd_Buf[k]));
            }
            DPRINTF(("\n"));
#endif
            if ((Cmd_Buf[POS_SYNC] != SYNC_BYTE) && (Cmd_Buf[POS_SYNC] != SYNC_BYTE_CP)) {
                EPRINTF(("Not Sync byte.\n"));
                Rx_State =  RX_RST;
                break;
            }

            pucFrame = &Cmd_Buf[0];

            if (Cmd_Buf[POS_SYNC] ==  SYNC_BYTE_CP) {
                uint8_t sum = Get_CheckSum8(pucFrame, Rx_len - 1);
                compatible  = 1;
                if (sum != Cmd_Buf[Rx_len - 1]) {
                    EPRINTF(("CheckSum Error.\n"));
                    Rx_State =  RX_RST;
                    break;
                }
            } else {
                if (Get_CRC8(Cmd_Buf, (int)Rx_len)) {
                    EPRINTF(("CRC Error.\n"));
                    ErrorCode = ECODE_CRC_ERROR   ;
                    Rx_State =  RX_RESP;
                    break;
                }
            }

            usLength = Rx_len - 1 - 2;
            Rx_State =  RX_EXE;
            break;

        case RX_EXE:
            if (Usart_Comm_TxBusy()) break;
            ucFunctionCode = pucFrame[1];
            ErrorCode = ECODE_NOT_SUPPORT_CMD   ;

            for (i = 0; i < (sizeof(InstCmdHandler) / sizeof(CmdHandler)); i++) {
                if ((InstCmdHandler[i].FunCode == ucFunctionCode)
                    && (InstCmdHandler[i].mode == compatible)) {
                    ErrorCode =  InstCmdHandler[i].CmdFuc(pucFrame, usLength);
                    break;
                }
            }

            if ((ErrorCode != ECODE_SUCCESS) && (compatible == 0)) {
                Rx_State =  RX_RESP;
                break;
            }

            Rx_State =  RX_SYNC;
            break;

        case RX_RESP:
            ComEcho_ECode(pucFrame, ErrorCode);
            Rx_State =  RX_SYNC;
            break;

        case RX_RST:
            Usart_Comm_ResetRx();
            Rx_State =  RX_SYNC;
            break;
    }
}


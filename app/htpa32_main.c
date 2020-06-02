
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "global.h"
#include "hal.h"
#include "i2c_drv.h"
#include "setting.h"
#include "htpa32_drv.h"
#include "htpa32_main.h"
#include "htpa32_def.h"
#include "htpa32_table.h"

#define LINE_BYTE  (32*2)
#define FILTER_TAG (16)

#define K_NUM 2
#define DIST_NUM 101

#define AVG_TH  (3000)

enum {
    HTRD_FRAME_IDLE = 0,
    HTRD_FRAME_START ,
    HTRD_START_CONV  ,
    HTRD_WAIT_CONV,
    HTRD_READ_TOP,
    HTRD_READ_BOTTOM,
    HTRD_GET_EOFFSET,
    HTRD_WAIT_EOFFSET,
    HTRD_EOFFSET_TOP,
    HTRD_EOFFSET_BOTTOM,
    HTRD_READ_DONE,
};

enum {
    HTCALCUL_IDLE = 0,
    HTCALCUL_START,
    HTCALCUL_ELE_OFFSET,
    HTCALCUL_VDD_COMP,
    HTCALCUL_SENS_COMP,
    HTCALCUL_LOOK_UP_TBL,
    HTCALCUL_SEARCH_OBJ,

};

enum {
    HTRECOVER_IDLE = 0,
    HTRECOVER_START,
    HTRECOVER_SENSOR_OFF,
    HTRECOVER_SENSOR_ON,
    HTRECOVER_I2C_RESET,
    HTRECOVER_SENSOR_WAKEUP,
    HTRECOVER_RD_CALIB,
    HTRECOVER_SETMBI,
    HTRECOVER_SETBIAS1,
    HTRECOVER_SETBIAS2,
    HTRECOVER_SETCLK,
    HTRECOVER_SETBPA1,
    HTRECOVER_SETBPA2,
    HTRECOVER_SETPU,
    HTRECOVER_DONE,
};

struct CalibData_t {
    uint32_t DeviceID;

    uint16_t tn;
    uint8_t gradScale;

    uint8_t SetMBITCalib;
    uint8_t SetBIASCalib;
    uint8_t SetCLKCalib;
    uint8_t SetBPACalib;
    uint8_t SetPUCalib;

    uint8_t Arraytype;

    uint16_t VDDTH1;
    uint16_t VDDTH2;

    uint16_t PTAT_TH1;
    uint16_t PTAT_TH2;

    float PTATGrad;
    float PTATOff;
    float PixCMin;
    float PixCMax;
    uint8_t epsilon;

    uint8_t VddScGrad;
    uint8_t VddScOff;

    uint8_t SetMBITUser;
    uint8_t SetBIASUser;
    uint8_t SetCLKUser;
    uint8_t SetBPAUser;
    uint8_t SetPUUser;

    int8_t GlobalOffset;
    uint16_t GlobalGain;

    uint8_t NrOfDefPix;
    uint8_t NrOfDefPix_Rd;
    uint8_t DeadMask[MAX_DEADPIXEL];
    uint16_t DeadPixAdr[24];

    int16_t VddCompGrad[EOFFSET_CNT];
    int16_t VddCompOff[EOFFSET_CNT];

    int16_t ThGrad[PIXEL];
    int16_t ThOff[PIXEL];

    int32_t PixC[PIXEL];

    int32_t gradScale_2pw;
    int32_t VddScGrad_2pw;
    int32_t VddScOff_2pw;
    int32_t resolution;
};

typedef union HtConvData_t {
    struct HtBlock_t {
        uint16_t ptat;
        uint16_t pixel[128];
    } line __attribute__((packed));
    uint8_t buffer[258];
} Ht_LineData ;

struct FrameData_t {
    uint8_t isptat;
    uint16_t ptat[8];
    uint16_t v0[1024];
    uint16_t elOffset[256];
};


struct ConvObj_t {
    uint8_t vdd_meas: 1;
    uint8_t vdd_valid: 1;
    uint8_t ptat_valid: 1;
    uint8_t block;
    int8_t readframe;
    int8_t processframe;
    uint32_t totalFrame;
    float PtatAve;
    float VddAve;
    int32_t ta;

    uint16_t objLastTemp;
    uint16_t voutAvg;
    uint16_t voutSD;
    int16_t avgCnt;
    uint16_t history[FILTER_TAG];
    int8_t startCnt;
    int8_t hisPos;

    int8_t read_state;
    int8_t process_state;
    int8_t recover_state;
    int32_t vout[1024];

    struct OutData_t  Output;
    struct FrameData_t *curFrameData;
    struct FrameData_t *procFrameData;
};


static void Htpa32_SeachObj(struct ConvObj_t *Obj);

struct ConvObj_t htObj = { 0 };

static uint16_t ke_data[K_NUM * K_NUM ];
static int16_t temp_data[1024];
static int16_t temp_dist[DIST_NUM];

Ht_LineData LineTop __attribute__((aligned(4))) = { 0 };
Ht_LineData LineBottom  __attribute__((aligned(4))) = { 0 };
struct FrameData_t FrameData[2];

struct CalibData_t SysCalib = { 0 };

static inline float mean(uint16_t *data, int len)
{
    int sum = 0;
    for (int i = 0; i < len; ++i) {
        sum += data[i];
    }
    return (float)((float)sum / (float)len);
}

static inline int32_t pow_2(uint32_t b)
{
    return 2 << (b - 1);
}

void Print_16BitHex(const char *title , uint16_t *buf, int32_t len)
{
    if (title) {
        DPRINTF(("%s\n", title));
    }
    for (int i = 0; i < len; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%04x ", buf[i]));
    }
    DPRINTF(("\n\n"));
}

void Print_16Bit(const char *title , int16_t *buf, int32_t len)
{
    if (title) {
        DPRINTF(("%s\n", title));
    }
    for (int i = 0; i < len; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%4d ", buf[i]));
    }
    DPRINTF(("\n\n"));
}

void Print_32BitHex(const char *title , int32_t *buf, int32_t len)
{
    if (title) {
        DPRINTF(("%s\n", title));
    }
    for (int i = 0; i < len; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%4x ", buf[i]));
    }
    DPRINTF(("\n\n"));
}

void Print_32Bit(const char *title , int32_t *buf, int32_t len)
{
    if (title) {
        DPRINTF(("%s\n", title));
    }
    for (int i = 0; i < len; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%4d ", buf[i]));
    }
    DPRINTF(("\n\n"));
}


uint8_t Htpa32_GetDeadp(void)
{
    return SysCalib.NrOfDefPix_Rd;
}

void Htpa32_PrintCalib(struct CalibData_t *cal)
{
    DPRINTF(("DeviceID:0x%08x TN:%d Arraytype:%d \n", cal->DeviceID, cal->tn, cal->Arraytype));
    DPRINTF(("SetupData: 0x%x 0x%x 0x%x 0x%x 0x%x \n", cal->SetMBITCalib, cal->SetBIASCalib, cal->SetCLKCalib, cal->SetBPACalib, cal->SetPUCalib));
    DPRINTF(("SetupDataUser: 0x%x 0x%x 0x%x 0x%x 0x%x \n", cal->SetMBITUser, cal->SetBIASUser, cal->SetCLKUser, cal->SetBPAUser, cal->SetPUUser));
    DPRINTF(("gradScale:%d  pow:%d \n", cal->gradScale, cal->gradScale_2pw));
    DPRINTF(("VddScGrad:%d  pow:%d \n", cal->VddScGrad, cal->VddScGrad_2pw));
    DPRINTF(("VddScOff:%d  pow:%d \n", cal->VddScOff, cal->VddScOff_2pw));
    DPRINTF(("VDD_TH1=%d VDD_TH2=%d \n", cal->VDDTH1, cal->VDDTH2));
    DPRINTF(("PTAT_TH1=%d PTAT_TH2=%d \n", cal->PTAT_TH1, cal->PTAT_TH2));
    DPRINTF(("GlobalOffset=%d VddScOff=%d \n", cal->GlobalOffset, cal->GlobalGain));
    DPRINTF(("Nr of Dead Pix =%d (%d) \n", cal->NrOfDefPix, cal->NrOfDefPix_Rd));
    DPRINTF(("PixCMax  = %d \n", (int32_t)(cal->PixCMax)));
    DPRINTF(("PixCMin  = %d \n", (int32_t)(cal->PixCMin)));
    DPRINTF(("epsilon =%d \n", cal->epsilon));
    DPRINTF(("PTATGradx1000000 = %d \n", (int32_t)(cal->PTATGrad * 1000000.0)));
    DPRINTF(("PTATOff = %d \n", (int32_t)(cal->PTATOff * 1.0)));
    DPRINTF(("ADC resolution:%d \n", SysCalib.resolution));


#if 0
    DPRINTF(("\n"));
    DPRINTF(("VddCompGrad:\n"));
    for (int i = 0; i < EOFFSET_CNT; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%8d ", SysCalib.VddCompGrad[i]));
    }
    DPRINTF(("\n\n"));
    DPRINTF(("VddCompOff:\n"));
    for (int i = 0; i < EOFFSET_CNT; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%8d ", SysCalib.VddCompOff[i]));
    }
    DPRINTF(("\n\n"));

    DPRINTF(("ThGrad:\n"));
    for (int i = 0; i < PIXEL; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%8d ", SysCalib.ThGrad[i]));
    }
    DPRINTF(("\n\n"));

    DPRINTF(("ThOffset:\n"));
    for (int i = 0; i < PIXEL; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%8d ", SysCalib.ThOff[i]));
    }
    DPRINTF(("\n\n"));

    DPRINTF(("PixC:\n"));
    for (int i = 0; i < PIXEL; i++) {
        if (i && ((i % 32) == 0)) DPRINTF(("\n"));
        DPRINTF(("%9d ", SysCalib.PixC[i]));
    }
    DPRINTF(("\n\n"));
#endif
}


void Htpa32_ReadCalibData(void)
{
    int i;
    uint16_t PixCRaw;
    uint8_t *pTmp = 0;
    uint16_t addr;

    Htpa32_EEPROM_ReadDone(ADR_PIXCMIN, (uint8_t *) & (SysCalib.PixCMin), 4);
    Htpa32_EEPROM_ReadDone(ADR_PIXCMAX, (uint8_t *) & (SysCalib.PixCMax), 4);

    Htpa32_EEPROM_ReadByte(ADR_GRADSCALE, &SysCalib.gradScale);
    Htpa32_EEPROM_ReadWord(ADR_TABLENUMBER, &SysCalib.tn);
    Htpa32_EEPROM_ReadByte(ADR_EPSILON, &SysCalib.epsilon);

    Htpa32_EEPROM_ReadByte(ADR_MBITPIXC, &SysCalib.SetMBITCalib);
    Htpa32_EEPROM_ReadByte(ADR_BIASPIXC, &SysCalib.SetBIASCalib);
    Htpa32_EEPROM_ReadByte(ADR_CLKPIXC, &SysCalib.SetCLKCalib);
    Htpa32_EEPROM_ReadByte(ADR_BPAPIXC, &SysCalib.SetBPACalib);
    Htpa32_EEPROM_ReadByte(ADR_PUPIXC, &SysCalib.SetPUCalib);

    Htpa32_EEPROM_ReadByte(ADR_ARRAYTYPE, &SysCalib.Arraytype);

    Htpa32_EEPROM_ReadWord(ADR_VDDTH1, &SysCalib.VDDTH1);
    Htpa32_EEPROM_ReadWord(ADR_VDDTH2, &SysCalib.VDDTH2);

    Htpa32_EEPROM_ReadDone(ADR_PTATGRAD, (uint8_t *) & (SysCalib.PTATGrad), 4);
    Htpa32_EEPROM_ReadDone(ADR_PTATOFFSET, (uint8_t *) & (SysCalib.PTATOff), 4);

    Htpa32_EEPROM_ReadDone(ADR_DEVID, (uint8_t *) & (SysCalib.DeviceID), 4);

    Htpa32_EEPROM_ReadWord(ADR_PTATTH1, &SysCalib.PTAT_TH1);
    Htpa32_EEPROM_ReadWord(ADR_PTATTH2, &SysCalib.PTAT_TH2);

    Htpa32_EEPROM_ReadByte(ADR_VDDSCGRAD, &SysCalib.VddScGrad);
    Htpa32_EEPROM_ReadByte(ADR_VDDSCOFF, &SysCalib.VddScOff);

    Htpa32_EEPROM_ReadByte(ADR_MBITUSER, &(SysCalib.SetMBITUser));
    if (SysCalib.SetMBITUser < 0x09 || SysCalib.SetMBITUser > 0x2c) {
        SysCalib.SetMBITUser = MBITRIMDefault;
    }

    Htpa32_EEPROM_ReadByte(ADR_BIASUSER , &(SysCalib.SetBIASUser));
    if (SysCalib.SetBIASUser == 0  || SysCalib.SetBIASUser > 0x1f) {
        SysCalib.SetBIASUser = BIAScurrentDefault    ;
    }

    Htpa32_EEPROM_ReadByte(ADR_CLKUSER, &(SysCalib.SetCLKUser));
    if (SysCalib.SetCLKUser > 0x3f) {
        SysCalib.SetCLKUser = CLKTRIMDefault;
    }

    Htpa32_EEPROM_ReadByte(ADR_BPAUSER, &(SysCalib.SetBPAUser));
    if (SysCalib.SetBPAUser == 0 || SysCalib.SetBPAUser > 0x1f) {
        SysCalib.SetBPAUser = BPATRMDefault;
    }

    Htpa32_EEPROM_ReadByte(ADR_PUUSER, &(SysCalib.SetPUUser));
    if (!((SysCalib.SetPUUser == 0x11) || (SysCalib.SetPUUser == 0x22) ||
          (SysCalib.SetPUUser == 0x44) || (SysCalib.SetPUUser == 0x88))) {
        SysCalib.SetPUUser = PUTRIMDefault;
    }

    Htpa32_EEPROM_ReadByte(ADR_GLOBALOFFSET, (uint8_t *) & (SysCalib.GlobalOffset));
    Htpa32_EEPROM_ReadWord(ADR_GLOBALGAIN, &(SysCalib.GlobalGain));
    Htpa32_EEPROM_ReadByte(ADR_NROFDEFPIX, &(SysCalib.NrOfDefPix_Rd));

    SysCalib.NrOfDefPix = SysCalib.NrOfDefPix_Rd;
    if (SysCalib.NrOfDefPix > MAX_DEADPIXEL) {
        EPRINTF(("Error at dead pixel. %d \n", SysCalib.NrOfDefPix));
        SysCalib.NrOfDefPix = 0;
    }

    if (SysCalib.NrOfDefPix) {
        Htpa32_EEPROM_ReadDone(ADR_DEADPIXADR, (uint8_t *)(SysCalib.DeadPixAdr), SysCalib.NrOfDefPix * sizeof(uint16_t));
        Htpa32_EEPROM_ReadDone(ADR_DEADMASK, (uint8_t *)(SysCalib.DeadMask), SysCalib.NrOfDefPix);
    }

    pTmp = (uint8_t *)(SysCalib.VddCompGrad);
    memset(pTmp, 0, (EOFFSET_CNT * sizeof(uint16_t)));
    Htpa32_EEPROM_ReadDone(ADR_VDDCOMPGRAD , pTmp, (EOFFSET_CNT * sizeof(int16_t)) / 2); //top half
    pTmp += (EOFFSET_CNT * sizeof(int16_t)) / 2;
    addr = ADR_VDDCOMPGRAD + (EOFFSET_CNT * sizeof(int16_t)) - LINE_BYTE;
    for (i = 0; i < 4; i++) {
        Htpa32_EEPROM_ReadDone(addr, pTmp, LINE_BYTE);
        addr -= LINE_BYTE;
        pTmp += LINE_BYTE;
    }

    pTmp = (uint8_t *)(SysCalib.VddCompOff);
    memset(pTmp, 0, (EOFFSET_CNT * sizeof(uint16_t)));
    Htpa32_EEPROM_ReadDone(ADR_VDDCOMPOFF , pTmp, (EOFFSET_CNT * sizeof(uint16_t)) / 2);
    pTmp += (EOFFSET_CNT * sizeof(int16_t)) / 2;
    addr = ADR_VDDCOMPOFF + (EOFFSET_CNT * sizeof(int16_t)) - LINE_BYTE;
    for (i = 0; i < 4; i++) {
        Htpa32_EEPROM_ReadDone(addr, pTmp, LINE_BYTE);
        addr -= LINE_BYTE;
        pTmp += LINE_BYTE;
    }

    pTmp = (uint8_t *)(SysCalib.ThGrad); //use as temp buffer;
    memset(pTmp, 0, (PIXEL * sizeof(uint16_t)));
    Htpa32_EEPROM_ReadDone(ADR_PIXC , pTmp, PIXEL * sizeof(int16_t) / 2);
    pTmp += (PIXEL * sizeof(int16_t)) / 2;
    addr = ADR_PIXC + sizeof(int16_t) * PIXEL - LINE_BYTE;
    for (i = 0; i < 16; i++) {
        Htpa32_EEPROM_ReadDone(addr, pTmp, LINE_BYTE);
        addr -= LINE_BYTE;
        pTmp += LINE_BYTE;
    }

    for (i = 0; i < PIXEL; i++) {
        PixCRaw = (uint16_t)SysCalib.ThGrad[i];
        SysCalib.PixC[i] = (int32_t)(((((float)PixCRaw) * ((SysCalib.PixCMax - SysCalib.PixCMin) / 65535.0)) + SysCalib.PixCMin) * (float)(SysCalib.epsilon) / 100.0 * (float)(SysCalib.GlobalGain) / 10000.0 + 0.5);
    }

    pTmp = (uint8_t *)(SysCalib.ThGrad);
    memset(pTmp, 0, (PIXEL * sizeof(uint16_t)));
    Htpa32_EEPROM_ReadDone(ADR_THGRAD , pTmp, PIXEL * sizeof(int16_t) / 2);
    pTmp += (PIXEL * sizeof(int16_t)) / 2;
    addr = ADR_THGRAD  + sizeof(int16_t) * PIXEL - LINE_BYTE;
    for (i = 0; i < 16; i++) {
        Htpa32_EEPROM_ReadDone(addr, pTmp, LINE_BYTE);
        addr -= LINE_BYTE;
        pTmp += LINE_BYTE;
    }

    pTmp = (uint8_t *)(SysCalib.ThOff);
    memset(pTmp, 0, (PIXEL * sizeof(uint16_t)));
    Htpa32_EEPROM_ReadDone(ADR_THOFFSET , pTmp, PIXEL * sizeof(int16_t) / 2);
    pTmp += (PIXEL * sizeof(int16_t)) / 2;
    addr = ADR_THOFFSET  + sizeof(int16_t) * PIXEL - LINE_BYTE;
    for (i = 0; i < 16; i++) {
        Htpa32_EEPROM_ReadDone(addr, pTmp, LINE_BYTE);
        addr -= LINE_BYTE;
        pTmp += LINE_BYTE;
    }

    SysCalib.gradScale_2pw = pow_2(SysCalib.gradScale);
    SysCalib.VddScGrad_2pw = pow_2(SysCalib.VddScGrad);
    SysCalib.VddScOff_2pw = pow_2(SysCalib.VddScOff);
    SysCalib.resolution = (SysCalib.SetMBITCalib & 0xf) + 4;

    Htpa32_PrintCalib(&SysCalib);
    return ;
}

static void Htpa32_ResetObj(struct ConvObj_t  *obj)
{
    obj->read_state = HTRD_FRAME_IDLE ;
    obj->process_state = HTCALCUL_IDLE ;
    obj->vdd_meas = 0;
    obj->vdd_valid = 0;
    obj->ptat_valid = 0;
    obj->block = 0;
    obj->readframe = 0;
    obj->processframe = 0;
    obj->PtatAve = 0;
    obj->VddAve = 0;
    obj->ta = 0;
    obj->startCnt = 0;
    obj->hisPos = 0;
    obj->Output.valid = 0;
    obj->voutAvg = 0;
    obj->avgCnt = 0;
    obj->voutSD = 0;
    obj->objLastTemp = 0;
    memset(obj->history, 0, sizeof(uint16_t)* FILTER_TAG);

    return ;
}


void Htpa32_Init(void)
{
    int16_t offset;
    Htpa32_Drv_Init();

    Htpa32_WriteReg(REG_CONFIG, 0x01);
    DelayMS(5);
    Htpa32_ReadCalibData();
    DelayMS(5);
    Htpa32_WriteReg(REG_MBIT, SysCalib.SetMBITCalib);
    DelayMS(5);
    Htpa32_WriteReg(REG_BIASL, SysCalib.SetBIASCalib);
    DelayMS(5);
    Htpa32_WriteReg(REG_BIASR, SysCalib.SetBIASCalib);
    DelayMS(5);
    Htpa32_WriteReg(REG_CLK, SysCalib.SetCLKCalib);
    DelayMS(5);
    Htpa32_WriteReg(REG_BPAL, SysCalib.SetBPACalib);
    DelayMS(5);
    Htpa32_WriteReg(REG_BPAR, SysCalib.SetBPACalib);
    DelayMS(5);
    Htpa32_WriteReg(REG_PU, SysCalib.SetPUCalib);
    DelayMS(10);
    htObj.read_state = HTRD_FRAME_START ;
    Htpa32_SetOffset(SysCalib.GlobalOffset);
    offset = Sys_GetUsrOffset();
    Htpa32_SetUserOffset(offset);
}



void Htpa32_Service(void)
{
    int i, j;
    uint8_t cmd;
    int32_t start;
    int32_t tmp1, tmp2;

    static ticks_t  time2check = 0;

    switch (htObj.read_state) {
        case HTRD_FRAME_IDLE :
            break;

        case HTRD_FRAME_START :
            htObj.curFrameData = &FrameData[htObj.readframe];
            if (htObj.vdd_meas == 0) {
                htObj.curFrameData->isptat = 1;
            } else {
                htObj.curFrameData->isptat = 0;
            }
            htObj.read_state = HTRD_START_CONV;
            break;

        case HTRD_START_CONV:
            cmd = (htObj.vdd_meas << 2) | (htObj.block << 4) | 0x09;
            Htpa32_WriteReg(REG_CONFIG, cmd);
            htObj.read_state = HTRD_WAIT_CONV;
            break;

        case HTRD_WAIT_CONV:
            if ((Htpa32_GetStatus() & 0x01) == 0) break;
            Htpa32_ReadData(REG_READ1, LineTop.buffer, 258);
            htObj.read_state = HTRD_READ_TOP;
            break;

        case HTRD_READ_TOP:
            if (Htpa32_isIdle() == 0) break;
            Htpa32_ReadData(REG_READ2, LineBottom.buffer, 258);
            htObj.curFrameData->ptat[htObj.block] = bswap16(LineTop.line.ptat);
            start = (int32_t)(htObj.block) * 128;
            for (i = 0; i < 128; i++) {
                htObj.curFrameData->v0[start + i] = bswap16(LineTop.line.pixel[i]);
            }
            htObj.read_state = HTRD_READ_BOTTOM;
            break;

        case HTRD_READ_BOTTOM:
            if (Htpa32_isIdle() == 0) break;
            htObj.curFrameData->ptat[7 - htObj.block ] = bswap16(LineBottom.line.ptat);
            start = (8 - htObj.block) * 128 - 32;
            for (i = 0; i < 4; i++) {
                for (j = 0; j < 32; j++) {
                    htObj.curFrameData->v0[start + j] = bswap16(LineBottom.line.pixel[i * 32 + j]);
                }
                start -= 32;
            }

            htObj.block ++;
            if (htObj.block >= 4) {
                htObj.block = 0;
                htObj.read_state = HTRD_GET_EOFFSET;
                break;
            }
            htObj.read_state = HTRD_START_CONV;
            break;

        case HTRD_GET_EOFFSET:
            Htpa32_WriteReg(REG_CONFIG, 0x0b);  //read electrical offsets
            htObj.read_state = HTRD_WAIT_EOFFSET;
            break;

        case HTRD_WAIT_EOFFSET:
            if ((Htpa32_GetStatus() & 0x01) == 0) break;
            Htpa32_ReadData(REG_READ1, LineTop.buffer, 258);
            htObj.read_state = HTRD_EOFFSET_TOP;
            break;

        case HTRD_EOFFSET_TOP:
            if (Htpa32_isIdle() == 0) break;
            Htpa32_ReadData(REG_READ2, LineBottom.buffer, 258);
            if (htObj.curFrameData->isptat) {
                htObj.PtatAve = mean(htObj.curFrameData->ptat, 8);
                htObj.ptat_valid = 1;
            } else {
                htObj.VddAve = mean(htObj.curFrameData->ptat, 8);
                htObj.vdd_valid = 1;
            }
            for (i = 0; i < 128; i++) {
                htObj.curFrameData->elOffset[i] = bswap16(LineTop.line.pixel[i]);
            }

            htObj.read_state = HTRD_EOFFSET_BOTTOM;
            break;

        case HTRD_EOFFSET_BOTTOM:
            if (Htpa32_isIdle() == 0) break;
            start = 256 - 32;
            for (i = 0; i < 4; i++) {
                for (j = 0; j < 32; j++) {
                    htObj.curFrameData->elOffset[start + j] = bswap16(LineBottom.line.pixel[i * 32 + j]);
                }
                start -= 32;
            }

            //            Print_16BitHex("elOffset:",htObj.curFrameData->elOffset,256);
            htObj.read_state = HTRD_READ_DONE;
            break;

        case HTRD_READ_DONE:
            if (htObj.process_state != HTCALCUL_IDLE) break;
            htObj.processframe = htObj.readframe;
            htObj.readframe = (htObj.readframe) ? 0 : 1;
            htObj.vdd_meas = (htObj.vdd_meas) ? 0 : 1;
            htObj.totalFrame ++;
            htObj.process_state = HTCALCUL_START;
            htObj.read_state = HTRD_FRAME_START;
            break;

    }

    switch (htObj.process_state) {
        case HTCALCUL_IDLE :

            break;

        case HTCALCUL_START:
            if ((htObj.ptat_valid == 0) || (htObj.vdd_valid == 0)) {
                htObj.process_state = HTCALCUL_IDLE;
                break;
            }

            htObj.procFrameData = &FrameData[htObj.processframe];
            htObj.ta = (int32_t)(htObj.PtatAve * SysCalib.PTATGrad + SysCalib.PTATOff);

            for (i = 0; i < 1024; i++) {
                htObj.vout[i] = (int32_t)((uint32_t)htObj.procFrameData->v0[i]) - (int32_t)((int64_t)SysCalib.ThGrad[i] * ((int64_t)htObj.PtatAve) / (SysCalib.gradScale_2pw)) - (int32_t)SysCalib.ThOff[i];
            }

            htObj.process_state = HTCALCUL_ELE_OFFSET;
            break;

        case HTCALCUL_ELE_OFFSET:
            for (i = 0; i < 512; i++) {
                htObj.vout[i] = htObj.vout[i] - htObj.procFrameData->elOffset[i % 128];
            }

            for (i = 512; i < 1024; i++) {
                htObj.vout[i] = htObj.vout[i] - htObj.procFrameData->elOffset[i % 128 + 128];
            }
            htObj.process_state = HTCALCUL_VDD_COMP;
            break;

        case HTCALCUL_VDD_COMP:
            tmp2 = (int32_t)htObj.VddAve - (int32_t)SysCalib.VDDTH1 - (((int32_t)SysCalib.VDDTH2 - (int32_t)SysCalib.VDDTH1) / ((int32_t)SysCalib.PTAT_TH2 - (int32_t)SysCalib.PTAT_TH1)) * ((int32_t)htObj.PtatAve - (int32_t)SysCalib.PTAT_TH1);

            for (i = 0; i < 512; i++) {
                tmp1 = ((int32_t)SysCalib.VddCompGrad[i % 128] * (int32_t)htObj.PtatAve / SysCalib.VddScGrad_2pw + SysCalib.VddCompOff[i % 128]) / SysCalib.VddScOff_2pw;
                htObj.vout[i] -=  tmp1 * tmp2;
            }

            for (i = 512; i < 1024; i++) {
                tmp1 = (((int32_t)SysCalib.VddCompGrad[i % 128 + 128] * (int32_t)htObj.PtatAve / SysCalib.VddScGrad_2pw + SysCalib.VddCompOff[i % 128 + 128]) / SysCalib.VddScOff_2pw);
                htObj.vout[i] -=  tmp1 * tmp2;
            }

            htObj.process_state = HTCALCUL_SENS_COMP;
            break;

        case HTCALCUL_SENS_COMP:
            for (i = 0; i < 1024; i++) {
                htObj.vout[i] = (int32_t)((int64_t)htObj.vout[i] * (int64_t) HTPA_PCSCALEVAL / SysCalib.PixC[i]) ;
            }

            htObj.process_state = HTCALCUL_LOOK_UP_TBL;
            break;

        case HTCALCUL_LOOK_UP_TBL:
            htObj.Output.ta = (int16_t)htObj.ta;
            Htpa32_SearchTbl(&(htObj.Output), htObj.vout);
            htObj.process_state = HTCALCUL_SEARCH_OBJ;
            break;

        case HTCALCUL_SEARCH_OBJ:
            Htpa32_SeachObj(&htObj);
            htObj.process_state = HTCALCUL_IDLE;
            break;

    }

    switch (htObj.recover_state) {
        case HTRECOVER_IDLE :
            break;

        case HTRECOVER_START:
            Htpa32_ResetObj(&htObj);
            htObj.recover_state = HTRECOVER_SENSOR_OFF;
            break;

        case HTRECOVER_SENSOR_OFF:
            Sensor_Ctrl_off();
            time2check = Hal_GetTicks() + MS_TO_TICKS(200);
            htObj.recover_state = HTRECOVER_SENSOR_ON;
            break;

        case HTRECOVER_SENSOR_ON:
            if (Hal_GetTicks() < time2check) break;
            Sensor_Ctrl_on();
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_I2C_RESET;
            break;

        case HTRECOVER_I2C_RESET:
            if (Hal_GetTicks() < time2check) break;
            mI2C_Reset();
            time2check = Hal_GetTicks() + MS_TO_TICKS(1);
            htObj.recover_state = HTRECOVER_SENSOR_WAKEUP;
            break;

        case HTRECOVER_SENSOR_WAKEUP:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_CONFIG, 0x01);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_RD_CALIB;
            break;

        case HTRECOVER_RD_CALIB:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_ReadCalibData();
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETMBI;
            break;

        case HTRECOVER_SETMBI:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_MBIT, SysCalib.SetMBITCalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETBIAS1;
            break;

        case HTRECOVER_SETBIAS1:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_BIASL, SysCalib.SetBIASCalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETBIAS2;
            break;

        case HTRECOVER_SETBIAS2:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_BIASR, SysCalib.SetBIASCalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETCLK;
            break;

        case HTRECOVER_SETCLK:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_CLK, SysCalib.SetCLKCalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETBPA1;
            break;

        case HTRECOVER_SETBPA1:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_BPAL, SysCalib.SetBPACalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETBPA2;
            break;

        case HTRECOVER_SETBPA2:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_BPAR, SysCalib.SetBPACalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_SETPU;
            break;

        case HTRECOVER_SETPU:
            if (Hal_GetTicks() < time2check) break;
            Htpa32_WriteReg(REG_PU, SysCalib.SetPUCalib);
            time2check = Hal_GetTicks() + MS_TO_TICKS(5);
            htObj.recover_state = HTRECOVER_DONE;
            break;

        case HTRECOVER_DONE:
            if (Hal_GetTicks() < time2check) break;
            htObj.read_state = HTRD_FRAME_START ;
            htObj.recover_state = HTRECOVER_IDLE;
            break;
    }

    if (mI2C_BusError()) {
        htObj.recover_state = HTRECOVER_START;
    }
}



void Htpa32_Process_DeadPix(int32_t *buf)
{
    int32_t pixsum = 0;
    int32_t maskcnt = 0;
    int16_t x, y;
    int8_t valid = 0;

    if ((SysCalib.NrOfDefPix == 0) || (SysCalib.NrOfDefPix > MAX_DEADPIXEL)) {
        return ;
    }

    for (int i = 0; i < SysCalib.NrOfDefPix; i++) {
        pixsum = 0;
        maskcnt = 0;
        x = SysCalib.DeadPixAdr[i] % 32;
        y = SysCalib.DeadPixAdr[i] / 32;
        if (SysCalib.DeadPixAdr[i] < PIXEL / 2) {
            for (int k = 0; k < 8; k ++) {
                valid = 0;
                if ((SysCalib.DeadMask[i] >> k) & 0x01) {
                    switch (k) {
                        case 0:
                            y -= 1;
                            if (y >= 0)  valid = 1;
                            break;
                        case 1:
                            x += 1;
                            y -= 1;
                            if (x < 32 && y >= 0) valid = 1;
                            break;
                        case 2:
                            x += 1;
                            if (x < 32) valid = 1;
                            break;
                        case 3:
                            x += 1;
                            y += 1;
                            if (x < 32) valid = 1;
                            break;
                        case 4:
                            y += 1;
                            valid = 1;
                            break;
                        case 5:
                            x -= 1;
                            y += 1;
                            if (x >= 0) valid = 1;
                            break;
                        case 6:
                            x -= 1;
                            if (x >= 0) valid = 1;
                            break;
                        case 7:
                            x -= 1;
                            y -= 1;
                            if ((x >= 0) && (y >= 0)) valid = 1;
                    }
                    if (valid) {
                        pixsum  += buf[y * 32 + x];
                        maskcnt++;
                    }
                }
            }
            if (maskcnt) {
                buf[SysCalib.DeadPixAdr[i]] = pixsum / maskcnt;
            }
        } else {
            for (int k = 0; k < 8; k ++) {
                valid = 0;
                if ((SysCalib.DeadMask[i] >> k) & 0x01) {
                    switch (k) {
                        case 0:
                            y += 1;
                            if (y < 32)  valid = 1;
                            break;
                        case 1:
                            x += 1;
                            y -= 1;
                            if (x < 32 && y >= 0) valid = 1;
                            break;
                        case 2:
                            x += 1;
                            if (x < 32) valid = 1;
                            break;
                        case 3:
                            x += 1;
                            y -= 1;
                            if (x < 32) valid = 1;
                            break;
                        case 4:
                            y -= 1;
                            valid = 1;
                            break;
                        case 5:
                            x -= 1;
                            y -= 1;
                            if (x >= 0) valid = 1;
                            break;
                        case 6:
                            x -= 1;
                            if (x >= 0) valid = 1;
                            break;
                        case 7:
                            x += 1;
                            y += 1;
                            if ((x < 32) && (y < 32)) valid = 1;
                    }
                    if (valid) {
                        pixsum  += buf[y * 32 + x];
                        maskcnt++;
                    }
                }
            }
            if (maskcnt) {
                buf[SysCalib.DeadPixAdr[i]] = pixsum / maskcnt;
            }
        }

    }

}

struct OutData_t *Htpa32_GetResult(void)
{
    return &(htObj.Output);
}


static void Htpa32_SeachObj(struct ConvObj_t *Obj)
{
    int i, j;
    int ki, kj;
    int n;
    int32_t sum;
    int32_t sd;
    int32_t avg;
    int32_t sum_total = 0;
    int32_t avg_total = 0;
    int32_t cnt_total = 0;
    int32_t ssd_i;

    static  int16_t up = 0;
    static  int16_t down = 0;

    uint16_t max_temp = 0;
    uint16_t max2_temp = 0;
    //    int8_t max_x, max_y;
    int8_t max2_x, max2_y;
    uint16_t temp_avg;
    uint16_t d0;


    int32_t counter = 0;
    uint16_t *tempbuf;

    tempbuf = (uint16_t *)(Obj->Output.out);

    memset(temp_dist, 0, DIST_NUM * 2);

    for (i = 3; i < 27; i++) {
        for (j = 3; j < 27; j ++) {
            sum = 0;
            sd = 0;
            n = 0;
            for (ki = 0; ki < K_NUM; ki++) {
                for (kj = 0; kj < K_NUM; kj++) {
                    ke_data[n] = tempbuf[(i + ki) * 32 + j + kj];
                    sum += ke_data[n];
                    n++;
                }
            }
            avg = sum / (K_NUM * K_NUM);
            sd = 0;
            for (ki = 0; ki < (K_NUM * K_NUM); ki++) {
                sd += abs(ke_data[ki] - avg);
            }

            temp_avg = (uint16_t) avg;

            d0 = temp_avg / 100;
            if (d0 <= 0)
                temp_dist[0]++;
            else if (d0 >= 100)
                temp_dist[100] ++;
            else temp_dist[d0] ++;

            temp_data[cnt_total] = (int16_t)avg;
            sum_total += avg;
            cnt_total ++;

            if (sd < avg) {
                if (temp_avg > max_temp) {
                    max_temp = temp_avg;
                    //max_x = j;
                    //max_y = i;
                } else if (temp_avg > max2_temp) {
                    max2_temp = temp_avg;
                    max2_x = j;
                    max2_y = i;
                }
            }
            if (avg > 3400 && avg < 6000) counter ++;
        }
    }

    avg_total  = sum_total / cnt_total;

    Obj->voutAvg = avg_total;
    Obj->Output.valid = 1;

    sum_total = 0;
    for (i = 0; i < cnt_total; i++) {
        sum_total += ((temp_data[i] - avg_total) * (temp_data[i] - avg_total));
    }
    sum_total /= cnt_total;
    ssd_i = (int32_t)(sqrt((float) sum_total));

    if ((counter == 0) && (ssd_i < 150)) {
        Obj->Output.valid = 0;
    } else if (counter > 120) {
        max_temp -= 180;
        max2_temp -= 180;
    } else if (counter > 300) {
        max_temp -= 250;
        max2_temp -= 250;
    } else if (counter > 400) {
        max_temp -= 400;
        max2_temp -= 400;
    }

    Obj->history[Obj->hisPos] = max2_temp;
    Obj->Output.obj_x = max2_x;
    Obj->Output.obj_y = max2_y;

    sum = 0;
    for (i = 0; i < FILTER_TAG; i++) {
        sum += Obj->history[i];
    }

    if (Obj->startCnt < FILTER_TAG) {
        Obj->startCnt ++;
        Obj->Output.objtemp = (sum / Obj->startCnt);
    } else {
        Obj->Output.objtemp = (sum / FILTER_TAG);
        if ((Obj->Output.objtemp > 3500 && Obj->Output.objtemp < 4000) && (Obj->voutAvg < 3900)) {
            if (Obj->Output.objtemp > 3610 && Obj->Output.objtemp < 3750) {
                if (Obj->Output.objtemp - Obj->objLastTemp < 200) {
                    if (Obj->Output.objtemp - Obj->objLastTemp > 0) {
                        up++;
                        down = 0;
                        if (up > 8) {
                            Obj->Output.objtemp = Obj->objLastTemp + 1;
                            up = 0;
                        } else {
                            Obj->Output.objtemp = Obj->objLastTemp ;
                        }
                        Obj->history[Obj->hisPos] = Obj->Output.objtemp;
                    }
                } else if (Obj->objLastTemp - Obj->Output.objtemp > 1) {
                    if (Obj->objLastTemp - Obj->Output.objtemp < 200) {
                        down ++;
                        up = 0;
                        if (down > 5) {
                            Obj->Output.objtemp = Obj->objLastTemp - 1;
                            down = 0;
                        } else {
                            Obj->Output.objtemp = Obj->objLastTemp;
                        }
                        Obj->history[Obj->hisPos] = Obj->Output.objtemp;
                    }
                }
            } else {
                if (Obj->Output.objtemp - Obj->objLastTemp > 4) {
                    if (Obj->Output.objtemp - Obj->objLastTemp < 300) {
                        Obj->Output.objtemp = Obj->objLastTemp + 4;
                        Obj->history[Obj->hisPos] = Obj->Output.objtemp;
                    }
                } else if (Obj->objLastTemp - Obj->Output.objtemp > 4) {
                    if (Obj->objLastTemp - Obj->Output.objtemp < 300) {
                        Obj->Output.objtemp = Obj->objLastTemp - 4;
                        Obj->history[Obj->hisPos] = Obj->Output.objtemp;
                    }
                }
            }
        }
    }

    Obj->hisPos ++;
    if (Obj->hisPos >= FILTER_TAG)
        Obj->hisPos = 0;

    Obj->objLastTemp = Obj->Output.objtemp;
    Obj->Output.avgtemp = Obj->voutAvg;

    if ((abs(AVG_TH - Obj->voutAvg) > 200) && (counter == 0)) {
        Obj->avgCnt++;
        if (Obj->avgCnt > 200) {
            // Htpa32_SetOffset((AVG_TH-Obj->voutAvg)/10);
            Obj->avgCnt = 0;
        }
    } else {
        Obj->avgCnt = 0;
    }

#if 1
    for (i = 25; i < 45; i++) {
        printf("%3d ", temp_dist[i]);
    }
    printf("  \n");
    printf("ObjTemp =[ %4d ] x=%2d y=%2d  avg=%d all_cnt=%d cnt=%d ta =%d ssd=%d \n", Obj->Output.objtemp, Obj->Output.obj_x, Obj->Output.obj_y, Obj->voutAvg, cnt_total, counter, Obj->Output.ta, ssd_i);
#endif
    return ;
}



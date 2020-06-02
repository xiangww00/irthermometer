
#ifndef __HTPA32_DEF_H__
#define __HTPA32_DEF_H__

#define ADR_PIXCMIN         0x00
#define ADR_PIXCMAX         0x04

#define ADR_GRADSCALE       0x08

#define ADR_TABLENUMBER     0x0B
#define ADR_EPSILON         0x0D

#define ADR_MBITPIXC        0x1A
#define ADR_BIASPIXC        0x1B
#define ADR_CLKPIXC         0x1C
#define ADR_BPAPIXC         0x1D
#define ADR_PUPIXC          0x1E

#define ADR_ARRAYTYPE       0x22

#define ADR_VDDTH1          0x26
#define ADR_VDDTH2          0x28

#define ADR_PTATGRAD        0x34
#define ADR_PTATOFFSET      0x38

#define ADR_PTATTH1         0x3C
#define ADR_PTATTH2         0x3E

#define ADR_VDDSCGRAD       0x4E
#define ADR_VDDSCOFF        0x4F

#define ADR_GLOBALOFFSET    0x54
#define ADR_GLOBALGAIN      0x55

#define ADR_MBITUSER        0x60
#define ADR_BIASUSER        0x61
#define ADR_CLKUSER         0x62
#define ADR_BPAUSER         0x63
#define ADR_PUUSER          0x64

#define ADR_DEVID           0x74

#define ADR_NROFDEFPIX      0x7F
#define ADR_DEADPIXADR      0x80
#define ADR_DEADMASK        0xB0


#define ADR_VDDCOMPGRAD     0x340
#define ADR_VDDCOMPOFF      0x540
#define ADR_THGRAD          0x740
#define ADR_THOFFSET        0xF40
#define ADR_PIXC            0x1740



#define LINE              32
#define COLUMN            32
#define PIXEL             (LINE * COLUMN)
#define EOFFSET_CNT       256

#define MAX_DEADPIXEL     5
#define MAX_DEADPIXELADDR   24


#define MBITRIMDefault        0x2c
#define BIAScurrentDefault    0x05
#define CLKTRIMDefault        0x15
#define BPATRMDefault         0x0c
#define PUTRIMDefault         0x88

#define HTPA_PCSCALEVAL       (100000000)


#endif


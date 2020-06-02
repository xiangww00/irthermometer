
#ifndef __COM_ERROR_H_
#define __COM_ERROR_H_

typedef enum {
    ECODE_SUCCESS            = 0,
    ECODE_NOT_SUPPORT_CMD    = 1,
    ECODE_CRC_ERROR          = 2,
    ECODE_INVALID_PARAM      = 3,
    ECODE_EXE_ERROR          = 4,
    ECODE_NOTERASE_FLASH     = 5,
    ECODE_CMD_FAIL           = 6,
} eCode;



#endif





#ifndef __HTPA32_MAIN_H__
#define __HTPA32_MAIN_H__


struct OutData_t {
    int16_t ta;
    int16_t out[1024];
    int16_t sample[1024];//for debug
    uint16_t objtemp;
    uint16_t avgtemp;
    int8_t obj_x;
    int8_t obj_y;
    int8_t valid;
};


void Htpa32_Init(void);
void Htpa32_Service(void);

void Htpa32_Process_DeadPix(int32_t *buf);
struct OutData_t *Htpa32_GetResult(void);
uint8_t Htpa32_GetDeadp(void);
#endif


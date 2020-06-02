
#ifndef __HTPA32_TABLE__
#define __HTPA32_TABLE__

void Htpa32_SetOffset(int16_t offset);
void Htpa32_SetUserOffset(int16_t offset);
int16_t Htpa32_GetUserOffset(void);
void Htpa32_SearchTbl(struct OutData_t *output, int32_t *inbuf);


#endif


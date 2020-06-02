
#ifndef __USART_H__
#define __USART_H__

enum {
    PORT_COM1 = 0,
    PORT_COM2 = 1,
    PORT_COM6 = 2,
    PORT_TOTAL = 3,
};

#define DEBUG_PORT PORT_COM2
#define COMM_PORT  PORT_COM1

void Usart_Debug_Init(void);
void Usart6_Tx(int32_t len);
void Usart1_Tx(int32_t len);
void Usart2_Tx(int32_t len);

void Usart_Reset_Rx(int8_t port);
int32_t Usart_Read(int8_t port, uint8_t *buf, int32_t size);
int32_t Usart_Get_RxSize(int8_t port);

int32_t Usart_Comm_GetSize(void);
int32_t Usart_Comm_Read(uint8_t *buf, int32_t size);
int8_t Usart_Comm_WaitTx(int32_t timeout);
int8_t Usart_Comm_TxBusy(void);
void Usart_Comm_Write(uint8_t *buf, int32_t len);
void Usart_Comm_ResetRx(void);
void Usart_Comm_Init(void);


#endif


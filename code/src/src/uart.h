#pragma once
#include <stdint.h>

#define UART_BUFF_SIZE 512

typedef struct {
  unsigned int dma_len;
  unsigned char data[UART_BUFF_SIZE];
} uart_data_t __attribute__((aligned(4)));

extern uart_data_t uart_rx_buff;
extern uart_data_t uart_tx_buff;

void init_uart(void);
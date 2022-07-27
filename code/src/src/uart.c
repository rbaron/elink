#include "uart.h"

#include <stdint.h>

#include "drivers.h"
#include "drivers/8258/flash.h"
#include "main.h"
#include "stack/ble/ble.h"
#include "tl_common.h"

uart_data_t uart_rx_buff = {0,
                            {
                                0,
                            }};
uart_data_t uart_tx_buff = {0,
                            {
                                0,
                            }};

void init_uart(void) {
  uart_recbuff_init((unsigned char*)&uart_rx_buff, sizeof(uart_rx_buff));
  uart_gpio_set(TXD, RXD);
  uart_reset();
  uart_init(12, 15, PARITY_NONE, STOP_BIT_ONE);  // baud rate: 115200
  uart_irq_enable(0, 0);
  uart_dma_enable(1, 1);
  irq_set_mask(FLD_IRQ_DMA_EN);
  dma_chn_irq_enable(FLD_DMA_CHN_UART_RX | FLD_DMA_CHN_UART_TX, 1);
}

void deinit_uart(void) {
  uart_reset();
}

_attribute_ram_code_ void puts(const char* str) {
  unsigned int len = strlen(str);
  uart_tx_buff.dma_len = len;
  memcpy(uart_tx_buff.data, str, len);
  uart_dma_send((unsigned char*)&uart_tx_buff);
}
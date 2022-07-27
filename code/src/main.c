#include "main.h"

#include <stdint.h>

#include "app.h"
#include "app_config.h"
#include "battery.h"
#include "ble.h"
#include "cmd_parser.h"
#include "drivers.h"
#include "drivers/8258/gpio_8258.h"
#include "elink.h"
#include "elink_config.h"
#include "epd.h"
#include "flash.h"
#include "i2c.h"
#include "led.h"
#include "nfc.h"
#include "ota.h"
#include "stack/ble/ble.h"
#include "tl_common.h"
#include "uart.h"
#include "vendor/common/user_config.h"

RAM unsigned char uart_dma_irqsrc;

_attribute_ram_code_ void irq_handler(void) {
#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  irq_blt_sdk_handler();
#endif  // ELINK_CFG_NODE_TYPE_BLE

  uart_dma_irqsrc = dma_chn_irq_status_get();

  if (uart_dma_irqsrc & FLD_DMA_CHN_UART_RX) {
    dma_chn_irq_status_clr(FLD_DMA_CHN_UART_RX);
    // printf("[irq_handler] Received %u bytes\n", uart_rx_buff.dma_len);

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_UART
    elink_handle_uart_rx(&uart_rx_buff);
#endif  // ELINK_CFG_NODE_TYPE_UART
  }
  if (uart_dma_irqsrc & FLD_DMA_CHN_UART_TX) {
    gpio_toggle(LED_BLUE);
    dma_chn_irq_status_clr(FLD_DMA_CHN_UART_TX);
  }
}

// Must be in RAM code.
_attribute_ram_code_ int main(void) {
  blc_pm_select_internal_32k_crystal();
  cpu_wakeup_init();

  // Determine whether or not we are waking up from deep sleep.
  int deepRetWakeUp = pm_is_MCU_deepRetentionWakeup();

  rf_drv_init(RF_MODE_BLE_1M);
  gpio_init(!deepRetWakeUp);
#if (CLOCK_SYS_CLOCK_HZ == 16000000)
  clock_init(SYS_CLK_16M_Crystal);
#elif (CLOCK_SYS_CLOCK_HZ == 24000000)
  clock_init(SYS_CLK_24M_Crystal);
#endif

  blc_app_loadCustomizedParameters();

  init_led();
#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_UART
  init_uart();
#endif
  init_i2c();

  if (deepRetWakeUp) {
    user_init_deep_sleep();
  } else {
    user_init_normal();
  }
  irq_enable();

  while (1) {
    main_loop();
  }
}

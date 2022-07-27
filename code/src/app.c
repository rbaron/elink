#include "app.h"

#include <stdint.h>

// #include "bart_tif.h"
#include "battery.h"
#include "ble.h"
#include "drivers.h"
#include "elink.h"
#include "elink_config.h"
#include "epd.h"
#include "epd_spi.h"
#include "flash.h"
#include "main.h"
#include "ota.h"
#include "stack/ble/ble.h"
#include "time.h"
#include "tl_common.h"
#include "vendor/common/blt_common.h"

RAM uint8_t battery_level;
RAM uint16_t battery_mv;
RAM int16_t temperature;

RAM uint8_t hour_refresh = 100;
RAM uint8_t minute_refresh = 100;

// Settings
extern settings_struct settings;

_attribute_ram_code_ static void shutdown_almost_all_gpio(void) {
  // except GPIO_PC5
  // None: 12 uA
  // All except GPIO_PC5: ~80 uA!
  // GROUPA & GROUPE: 3 uA!
  gpio_shutdown(GPIO_GROUPA);
  // gpio_shutdown(GPIO_GROUPB);
  // gpio_shutdown(GPIO_PC0);
  // gpio_shutdown(GPIO_PC1);
  // gpio_shutdown(GPIO_PC2);
  // gpio_shutdown(GPIO_PC3);
  // gpio_shutdown(GPIO_PC4);
  // gpio_shutdown(GPIO_PC6);
  // gpio_shutdown(GPIO_PC7);
  // gpio_shutdown(GPIO_GROUPD);
  gpio_shutdown(GPIO_GROUPE);
}

_attribute_ram_code_ void user_init_normal(void) {
  random_generator_init();
  init_time();
  init_flash();
  // init_nfc();
  elink_init();
  EPD_init();
  epd_set_sleep();

  // TEST
  // elink_set_state(GO_TO_SLEEP);

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  init_ble();
  // Tell UART nodes to go to sleep. Wait until they're fully booted.
  WaitMs(500);
  elink_ble_send_sleep_cmd_to_chain();
#endif  // ELINK_CFG_NODE_TYPE_BLE
}

_attribute_ram_code_ void user_init_deep_sleep(void) {
#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_UART && \
    ELINK_CFG_UART_SLEEP_MODE ==                       \
        ELINK_CFG_UART_SLEEP_MODE_DEEP_SLEEP_RETENTION

  // When waking up from deep sleep, we are ready to receive some data.
  elink_set_state(RECEIVING);
#endif

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  blc_ll_initBasicMCU();
  rf_set_power_level_index(RF_POWER_P3p01dBm);
  blc_ll_recoverDeepRetention();
#endif  // ELINK_CFG_NODE_TYPE_BLE
}

_attribute_ram_code_ void power_manage_uart_node(void) {
  if (elink_get_state() == GO_TO_SLEEP) {
    // printf("[power_manage_uart_node] Will go to sleep\n");

#if ELINK_CFG_UART_SLEEP_MODE == ELINK_CFG_UART_SLEEP_MODE_SUSPEND
    // Option 1 - SUSPEND_MODE. This is the "lightest" sleep mode, and consumes
    // ~60-100 uA. All RAM is retained, as well as GPIO states. Upon waking up,
    // the execution just continues from after the cpu_sleep_wakeup() call, as
    // if nothing happened. Set TX as gpio out and write high.
    // Repurpose RXD as wake up source.
    gpio_set_input_en(RXD, 0);
    gpio_setup_up_down_resistor(RXD, PM_PIN_PULLUP_1M);
    cpu_set_gpio_wakeup(RXD, Level_Low, 1);

    gpio_set_func(TXD, AS_GPIO);
    gpio_set_output_en(TXD, 1);
    gpio_set_input_en(TXD, 0);
    gpio_write(TXD, 0);
    // Do we need PM_WAKEUP_CORE here? Apparently not.
    cpu_sleep_wakeup(SUSPEND_MODE, PM_WAKEUP_PAD, 0);
    // In option 1 we need to re-init uart, since we temporarily repurposed the
    // RX and TX pins.
    init_uart();
    // We also need to make sure we're in the RECEIVING state, otherwise we'll
    // just go back to sleep.
    elink_set_state(RECEIVING);

#elif ELINK_CFG_UART_SLEEP_MODE == \
    ELINK_CFG_UART_SLEEP_MODE_DEEP_SLEEP_RETENTION
    // Option 2 - DEEP_SLEEP_RET_SRAM_LOW_16K/32K. Current ~2-3 uA. Only the
    // first 16/32K of RAM is retained (variables tagged with the `RAM` are
    // retained). Upon waking up, execution begins at the main() function, and
    // we can use pm_is_MCU_deepRetentionWakeup() to determined whether it's a
    // normal boot or deep sleep wake up. We may also use pm_is_deepPadWakeup().
    // If we want the TX -> RX_next line to remain high, we need an external
    // pullup, as the GPIOs will be set to a high-Z mode.
    // The following commented out code might be necessary to prevent leakage
    // current while sleeping, but it's causing a bleep in the TXD pin that
    // immediately wakes up the following node. gpio_set_func(RXD, AS_GPIO); Do
    // we need to de-init uart first? gpio_set_func(RXD, AS_GPIO);  // set rx
    // pin gpio_set_input_en(RXD, 0); dma_chn_irq_enable(FLD_DMA_CHN_UART_RX |
    // FLD_DMA_CHN_UART_TX, 0); gpio_set_output_en(RXD, 0);
    // gpio_set_input_en(RXD, 0);
    // gpio_setup_up_down_resistor(RXD, PM_PIN_PULLUP_1M);

    // There's a weird blip going on. At the first boot, when all nodes power up
    // at the same time and go to sleep at the "same" time, there's a blip in
    // the TX-RX line between the previous node and this one. Apparently it's
    // enough to wake this node up immediately. Adding this delay makes each
    // node wait for 10ms after the previous one went to sleep to set up the
    // GPIO wakeup. This seems to fix the issue, but at the cost that we need to
    // compile different firmwares for all nodes, which I was trying to avoid.
    // TODO: investigate this further and hopefully get rid of this sleep.
    // WaitMs(10 * ELINK_CFG_NODE_NUMBER);
    // Turn off the green led.
    gpio_write(LED_GREEN, 1);
    if (epd_state_handler()) {
      cpu_set_gpio_wakeup(EPD_BUSY, 1, 1);
      bls_pm_setWakeupSource(PM_WAKEUP_PAD);
      bls_pm_setSuspendMask(SUSPEND_DISABLE);
    } else {
      cpu_set_gpio_wakeup(RXD, Level_Low, 1);
      // gpio_shutdown(GPIO_ALL);
      shutdown_almost_all_gpio();
      // cpu_sleep_wakeup(DEEPSLEEP_MODE_RET_SRAM_LOW16K, PM_WAKEUP_PAD, 0);
      cpu_sleep_wakeup(DEEPSLEEP_MODE_RET_SRAM_LOW32K, PM_WAKEUP_PAD, 0);
    }
#endif  // ELINK_CFG_UART_SLEEP_MODE
  }
}

_attribute_ram_code_ void main_loop(void) {
  handler_time();

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  blt_sdk_main_loop();
#endif  // ELINK_CFG_NODE_TYPE_BLE

  elink_tick();

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_UART
  if (time_reached_period(Timer_CH_0, 1)) {
    gpio_toggle(LED_GREEN);
  }

  power_manage_uart_node();
#elif ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  if (epd_state_handler()) {
    cpu_set_gpio_wakeup(EPD_BUSY, 1, 1);
    bls_pm_setWakeupSource(PM_WAKEUP_PAD);
    bls_pm_setSuspendMask(SUSPEND_DISABLE);
  } else {
    shutdown_almost_all_gpio();
    blt_pm_proc();
  }
#endif
}

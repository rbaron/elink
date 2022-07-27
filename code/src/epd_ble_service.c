#include "epd_ble_service.h"

#include <stdint.h>
#include "tl_common.h"
#include "stack/ble/ble.h"
#include "epd.h"
#include "ble.h"
#include "uart.h"
#include "elink.h"

#define ASSERT_MIN_LEN(val, min_len) \
  if (val < min_len) {               \
    return 0;                        \
  }

int epd_ble_handle_write(void *p) {
  rf_packet_att_write_t *req = (rf_packet_att_write_t *)p;
  uint8_t *payload = &req->value;
  unsigned int payload_len = req->l2capLen - 3;

  ASSERT_MIN_LEN(payload_len, 1);

  uart_data_t data;
  data.dma_len = payload_len;
	for (int i = 0; i < payload_len; i++) {
		data.data[i] = payload[i];
	}

  elink_handle_uart_rx(&data);

  return 0;
}

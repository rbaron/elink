#include "elink.h"

#include "elink_config.h"
#include "epd.h"
#include "main.h"
#include "tl_common.h"

#define ELINK_MAGIC_BYTE 0x42

typedef enum {
  CLEAR = 0x00,
  WRITE = 0x01,
  DEBUG_PRINT = 0x02,
  // Sanity check test. Flashes an LED and forwards the same message.
  TEST_BYPASS = 0x03,
  // Draw a single char and update the display.
  DRAW_CHAR = 0x04,
  // Write a string to the buffer - does not update the display.
  DRAW_STR = 0x05,
  // Update the display with the current contents of the buffer.
  PUSH_BUF = 0x06,
  // Invert current buffer.
  INVERT = 0x07,
  // Request MCU to go to deep sleep.
  SLEEP = 0x08,
} Operation;

typedef struct {
  uint8_t magic;
  uint8_t to;
  uint8_t op;
  uint16_t data_len;
} elink_packet_header_t;

typedef struct {
  elink_packet_header_t header;
  __attribute__((aligned(4))) uint8_t data[ELINK_BUF_SIZE];
} elink_packet_t __attribute__((aligned(4)));

extern uint8_t epd_buffer[epd_buffer_size];
RAM elink_packet_t curr_packet = {
    {0, 0, 0, 0},
    {0xff},
};

RAM State state;

RAM int n_recv;

_attribute_ram_code_ int elink_init(void) {
  state = RECEIVING;
  n_recv = 0;
  return 0;
}

// Node that this function is called inside an interrupt handler. It has to
// return _fast_.
_attribute_ram_code_ int handle_rx_byte(uint8_t byte) {
  // printf("[elink_handle_uart_rx]: n_recv = %d, byte = 0x%02x, state = %d\n",
  //        n_recv, byte, state);

  if (state != RECEIVING) {
    return -1;
  }
  // printf("No more alerting\n");
  if (n_recv == 0) {
    // If the first byte is not the one we expect, bail out early.
    if (byte != ELINK_MAGIC_BYTE) {
      return -1;
    }
    curr_packet.header.magic = byte;
  } else if (n_recv == 1) {
    curr_packet.header.to = byte;
  } else if (n_recv == 2) {
    curr_packet.header.op = byte;
  } else if (n_recv == 3) {
    curr_packet.header.data_len = (byte << 8);
  } else if (n_recv == 4) {
    curr_packet.header.data_len |= byte;
    // printf("[handle_rx_byte] Current packet data size: %u\n",
    //        curr_packet.header.data_len);
    if (curr_packet.header.data_len == 0) {
      n_recv = 0;
      state = READY;
      return 0;
    }
  } else {
    // int data_idx = n_recv - sizeof(elink_packet_header_t);
    int data_idx = n_recv - 5;
    curr_packet.data[data_idx] = byte;
    if (data_idx == curr_packet.header.data_len - 1) {
      // printf("[handle_rx_byte] Received complete packet\n");
      n_recv = 0;
      state = READY;
      return 0;
    }
  }
  n_recv++;
  return n_recv;
}

#define UART_CHUNK_SIZE 128

static int relay_packet(const elink_packet_t *pkt) {
  // printf("[relay_packet] magic: %02x, to: %02x, op: %02x, data_len: %u\n",
  //        pkt->header.magic, pkt->header.to, pkt->header.op,
  //        pkt->header.data_len);
  uart_tx_buff.data[0] = curr_packet.header.magic;
  uart_tx_buff.data[1] = curr_packet.header.to;
  uart_tx_buff.data[2] = curr_packet.header.op;
  uart_tx_buff.data[3] = pkt->header.data_len >> 8;
  uart_tx_buff.data[4] = pkt->header.data_len & 0xff;
  uart_tx_buff.dma_len = 5;
  uart_dma_send((unsigned char *)&uart_tx_buff);

  // Split the data ito chunks and ship it through UART.
  for (int i = 0; i < curr_packet.header.data_len / UART_CHUNK_SIZE + 1; i++) {
    WaitMs(50);
    int idx0 = i * UART_CHUNK_SIZE;
    int len = min(UART_CHUNK_SIZE, curr_packet.header.data_len - idx0);
    uart_tx_buff.dma_len = len;
    memcpy(uart_tx_buff.data, curr_packet.data + idx0, len);
    uart_dma_send((unsigned char *)&uart_tx_buff);
  }
  return 0;
}

_attribute_ram_code_ int elink_handle_uart_rx(const uart_data_t *rx) {
  // printf("[elink_handle_uart_rx] Will handle new packet. rx->dma_len: %u\n",
  // rx->dma_len);
  if (state != RECEIVING || rx == NULL) {
    // printf("[elink_handle_uart_rx] Invalid state (%d) or rx null\n", state);
    return -1;
  }

  for (int i = 0; i < rx->dma_len; i++) {
    if (state == READY) {
      return 0;
    }

    handle_rx_byte(rx->data[i]);
  }
  return 0;
}

static inline EpdCharFont get_font(uint8_t font_byte) {
  switch (font_byte) {
    case 0x00:
      return SIXCAPS_120;
    case 0x01:
      return ROBOTO_MONO_100;
    case 0x02:
      return DIALOG_16;
    case 0x03:
      return SPECIAL_ELITE_30;
    case 0x04:
      return OPEN_SANS_8;
    case 0x05:
      return OPEN_SANS_BOLD_8;
    case 0x06:
      return OPEN_SANS_14;
    case 0x07:
      return OPEN_SANS_BOLD_14;
    case 0x08:
      return DSEG14_90;
    case 0x09:
      return PRESS_START_50;
    default:
      return SIXCAPS_120;
  }
}

static int handle_draw_char(const elink_packet_t *pkt) {
  EpdCharFont font = get_font(pkt->data[0]);
  epd_draw_char(pkt->data[1], font, pkt->data[2]);
  return 0;
}

static int handle_draw_str(const elink_packet_t *pkt) {
  /*
    Bytes in pkt->data:
      0: font
      1: unsigned baseline_y
      2: unsigned baseline_x
      3: NULL-terminated string
  */
  const epd_string_draw_specs_t specs = {
      .baseline_y = pkt->data[1] << 8 | pkt->data[2],
      .baseline_x = pkt->data[3] << 8 | pkt->data[4],
      .font = get_font(pkt->data[0]),
      .str = (char *)&pkt->data[5],
  };
  epd_draw_string(&specs);
  return 0;
}

static int handle_packet(elink_packet_t *pkt) {
  // If the packet is not mean for us, we decrement its address and pass it
  // along.
  if (pkt->header.to != 0x00) {
    pkt->header.to--;
    relay_packet(pkt);
    return 0;
  }

  // printf("[handle_packet] OP: 0x%02x\n", pkt->header.op);

  // The packet is for us.
  switch (pkt->header.op) {
    case WRITE:
      EPD_Display(pkt->data, sizeof(pkt->data), /*full_or_partial=*/1);
      return 0;
    case CLEAR:
      memset(epd_buffer, 0x00, 250 * 128 / 8);
      return 0;
    case PUSH_BUF:
      EPD_Display(epd_buffer, 250 * 128 / 8, /*full_or_partial=*/1);
      return 0;
    case TEST_BYPASS:
      gpio_toggle(LED_BLUE);
      return relay_packet(pkt);
    case DRAW_CHAR:
      return handle_draw_char(pkt);
    case DRAW_STR:
      return handle_draw_str(pkt);
    case INVERT:
      epd_invert_buff(epd_buffer, 250 * 128 / 8);
      return 0;
    default:
      return -1;
  }
}

int elink_tick(void) {
  // printf("[elink_tick]: state = %d\n", state);

  if (state != READY) {
    return 0;
  }
#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  init_uart();
#endif

  // printf("[elink_tick]: curr_packet.header.op = %d\n",
  // curr_packet.header.op);

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_UART
  if (curr_packet.header.op == SLEEP) {
    uint16_t wait_ms = curr_packet.data[0] << 8 | curr_packet.data[1];
    WaitMs(wait_ms);

    uint16_t new_wait_ms = 100;
    curr_packet.data[0] = new_wait_ms >> 8;
    curr_packet.data[1] = new_wait_ms & 0xff;
    handle_packet(&curr_packet);
    state = GO_TO_SLEEP;
    return 0;
  }
#endif  // ELINK_CFG_NODE_TYPE_UART

  handle_packet(&curr_packet);

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
  // Calling this increases the power consumption from 32 uA to a few mA.
  // Probably because one of the GPIOs toggle some EPD pins and it reboots and
  // goes out of deep sleep.
  // We can probably be more selective here if we really want, but I think it
  // should be fine.

  // gpio_shutdown(GPIO_ALL);
#endif

  // Clear image data.
  memset(curr_packet.data, 0xff, sizeof(curr_packet.data));

  state = RECEIVING;

  // Only UART nodes have their deep sleep state managed by the elink protocol
  // itself.
  // #if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_UART
  //   if (curr_packet.header.op == SLEEP) {
  //     state = GO_TO_SLEEP;
  //   }
  // #endif  // ELINK_CFG_NODE_TYPE_UART

  // printf("[elink_tick]: new state = %d\n", state);

  return 0;
}

State elink_get_state(void) {
  return state;
}

void elink_set_state(State s) {
  state = s;
}

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
void elink_ble_send_sleep_cmd_to_chain(void) {
  init_uart();
  uint16_t wait_ms = 5000;
  curr_packet.header.magic = ELINK_MAGIC_BYTE;
  curr_packet.header.to = 0xff;
  curr_packet.header.data_len = 2;
  curr_packet.header.op = SLEEP;
  curr_packet.data[0] = wait_ms >> 8;
  curr_packet.data[1] = wait_ms & 0xff;
  relay_packet(&curr_packet);
  gpio_shutdown(GPIO_ALL);
}
#endif
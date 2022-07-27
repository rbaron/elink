#pragma once

#include <stdint.h>

#include "uart.h"

/*  ELINK

# Power Management - Deep Sleep

## UART Nodes
* Boot in the RUNNING (elink == RECEIVING) state
### Going to sleep
When a message with op == SLEEP message is received:
  1. Wait for x seconds, so there's time for the previous node to go into deep
sleep
  2. Forward the message to the next node
  3. Immediately set the state to GO_TO_SLEEP
  4. In app::main_loop(), explicitly go to deep sleep, and set up to be awake by
a LOW level on its RX pin

### Waking up
UART nodes are woken up by a HIGH level on its RX pin. In practice, just the
fact that the previous node in the chain calls init_uart() is enough to cause a
blip in the prev-TX -> this-RX line, which automatically wakes up this one, and
so on until all nodes are awoken. If testing from a computer (with only UART, no
BLE nodes), sending any short message in the RX line will be enough to wake up.

## BLE Nodes
We don't explicitly manage the deep sleep of BLE nodes, since it can sleep while
the radio is still on. We rely on the built-in ble power management from the
SDK, so we never expli- citly set the node to sleep.
* Boot in the RUNNING (elink == RECECIVING) state
* Wait a little bit (so the next UART node boots up completely), sends a SLEEP
message to the next UART node, which will forward this message to the next->next
UART node, and so on until all UART nodes are sleeping.

*/

#define ELINK_WIDTH 250
#define ELINK_HEIGHT 128
#define ELINK_BUF_SIZE (ELINK_WIDTH * (ELINK_HEIGHT / 8))

typedef enum {
  RECEIVING,
  READY,
  GO_TO_SLEEP,
} State;

int elink_init(void);

int elink_handle_uart_rx(const uart_data_t *rx);

int elink_tick(void);

State elink_get_state(void);

void elink_set_state(State s);

#if ELINK_CFG_NODE_TYPE == ELINK_CFG_NODE_TYPE_BLE
void elink_ble_send_sleep_cmd_to_chain();
#endif
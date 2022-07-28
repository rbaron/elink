#pragma once

// BLE.
#define ELINK_CFG_NODE_TYPE_BLE 0x01
#define ELINK_CFG_NODE_TYPE_UART 0x02
#define ELINK_CFG_NODE_TYPE ELINK_CFG_NODE_TYPE_BLE
// #define ELINK_CFG_NODE_TYPE ELINK_CFG_NODE_TYPE_UART

// UART nodes.
// Sleeping modes.
#define ELINK_CFG_UART_SLEEP_MODE_SUSPEND 0x01
#define ELINK_CFG_UART_SLEEP_MODE_DEEP_SLEEP_RETENTION 0x02
#define ELINK_CFG_UART_SLEEP_MODE ELINK_CFG_UART_SLEEP_MODE_DEEP_SLEEP_RETENTION

// Debugging options.
// Fonts take a lot of space. Set this to 0 temporarily to generate much smaller
// builds for debugging unrelated issues.
#define ELINK_CFG_ENABLE_ALL_FONTS 1
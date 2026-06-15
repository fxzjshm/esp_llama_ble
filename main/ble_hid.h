// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2025, Input Labs Oy.

#pragma once
#include <stdint.h>

// BLE connection interval (1.25ms units): 6 = 7.5ms minimum
#define BLE_CONN_ITVL_MIN  6
#define BLE_CONN_ITVL_MAX  9

void ble_hid_init(void);
void ble_hid_send_hid(uint8_t report_id, uint8_t *data, uint8_t len);
void ble_hid_battery_set(uint8_t level);
void ble_hid_task_start_up(void);

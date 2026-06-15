// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2025, Input Labs Oy.

#pragma once
#include <stdint.h>

void ble_hid_init(void);
void ble_hid_send_hid(uint8_t report_id, uint8_t *data, uint8_t len);
void ble_hid_battery_set(uint8_t level);
void ble_hid_task_start_up(void);

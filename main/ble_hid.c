// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2025, Input Labs Oy.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_hidd.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_gap.h"
#include "host/ble_store.h"
#include "nimble/nimble_port.h"

extern void ble_store_config_init(void);
#include "nimble/nimble_port_freertos.h"
#include "esp_hid_gap.h"
#include "ble_hid.h"

static esp_hidd_dev_t *hid_dev = NULL;
static bool connected = false;
static struct ble_gap_event_listener gap_listener;

// HID Report Map — composite device with keyboard, mouse, and gamepad.
// Report ID 1: Keyboard (8 bytes: modifier, reserved, keycode[6])
// Report ID 2: Mouse (7 bytes: buttons, x[2], y[2], scroll, pan)
// Report ID 3: Gamepad (12 bytes: buttons[2], lx[2], ly[2], rx[2], ry[2], lz[1], rz[1])
static const uint8_t hid_report_map[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    0x05, 0x07,       //   Usage Page (Key Codes)
    0x19, 0xE0,       //   Usage Minimum (224)
    0x29, 0xE7,       //   Usage Maximum (231)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Const)
    0x95, 0x05,       //   Report Count (5)
    0x75, 0x01,       //   Report Size (1)
    0x05, 0x08,       //   Usage Page (LEDs)
    0x19, 0x01,       //   Usage Minimum (1)
    0x29, 0x05,       //   Usage Maximum (5)
    0x91, 0x02,       //   Output (Data,Var,Abs)
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x03,       //   Report Size (3)
    0x91, 0x01,       //   Output (Const)
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Key Codes)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0x65,       //   Usage Maximum (101)
    0x81, 0x00,       //   Input (Data,Array)
    0xC0,             // End Collection

    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x02,       // Usage (Mouse)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x02,       //   Report ID (2)
    0x09, 0x01,       //   Usage (Pointer)
    0xA1, 0x00,       //   Collection (Physical)
    0x05, 0x09,       //     Usage Page (Button)
    0x19, 0x01,       //     Usage Minimum (1)
    0x29, 0x05,       //     Usage Maximum (5)
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x95, 0x05,       //     Report Count (5)
    0x75, 0x01,       //     Report Size (1)
    0x81, 0x02,       //     Input (Data,Var,Abs)
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x03,       //     Report Size (3)
    0x81, 0x01,       //     Input (Const)
    0x05, 0x01,       //     Usage Page (Generic Desktop)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x16, 0x00, 0x80, //     Logical Minimum (-32767)
    0x26, 0xFF, 0x7F, //     Logical Maximum (32767)
    0x75, 0x10,       //     Report Size (16)
    0x95, 0x02,       //     Report Count (2)
    0x81, 0x06,       //     Input (Data,Var,Rel)
    0x09, 0x38,       //     Usage (Wheel)
    0x15, 0x81,       //     Logical Minimum (-127)
    0x25, 0x7F,       //     Logical Maximum (127)
    0x75, 0x08,       //     Report Size (8)
    0x95, 0x01,       //     Report Count (1)
    0x81, 0x06,       //     Input (Data,Var,Rel)
    0x09, 0x38,       //     Usage (Wheel) — pan
    0x15, 0x81,       //     Logical Minimum (-127)
    0x25, 0x7F,       //     Logical Maximum (127)
    0x75, 0x08,       //     Report Size (8)
    0x95, 0x01,       //     Report Count (1)
    0x81, 0x06,       //     Input (Data,Var,Rel)
    0xC0,             //   End Collection
    0xC0,             // End Collection

    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x05,       // Usage (Game Pad)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x03,       //   Report ID (3)
    0x05, 0x09,       //   Usage Page (Button)
    0x19, 0x01,       //   Usage Minimum (1)
    0x29, 0x10,       //   Usage Maximum (16)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x10,       //   Report Count (16)
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0x05, 0x01,       //   Usage Page (Generic Desktop)
    0x09, 0x30,       //   Usage (X)
    0x09, 0x31,       //   Usage (Y)
    0x09, 0x32,       //   Usage (Z)
    0x09, 0x35,       //   Usage (Rz)
    0x16, 0x00, 0x80, //   Logical Minimum (-32767)
    0x26, 0xFF, 0x7F, //   Logical Maximum (32767)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x04,       //   Report Count (4)
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0x09, 0x33,       //   Usage (Rx)
    0x09, 0x34,       //   Usage (Ry)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x02,       //   Report Count (2)
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0xC0              // End Collection
};

static esp_hid_raw_report_map_t maps[] = {{
    .data = hid_report_map,
    .len = sizeof(hid_report_map),
}};

static void hid_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    (void)event_data;
    switch (event_id) {
        case ESP_HIDD_START_EVENT:
            printf("BLE: HID device started\n");
            esp_hid_ble_gap_adv_start();
            break;
        case ESP_HIDD_CONNECT_EVENT:
            connected = true;
            printf("BLE: connected\n");
            break;
        case ESP_HIDD_DISCONNECT_EVENT:
            connected = false;
            printf("BLE: disconnected\n");
            esp_hid_ble_gap_adv_start();
            break;
        case ESP_HIDD_OUTPUT_EVENT:
            break;
        case ESP_HIDD_FEATURE_EVENT:
            break;
        default:
            break;
    }
}

static int ble_gap_event_handler(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_CONNECT && event->connect.status == 0) {
        struct ble_gap_upd_params params = {
            .itvl_min = BLE_CONN_ITVL_MIN,
            .itvl_max = BLE_CONN_ITVL_MAX,
            .latency = 0,
            .supervision_timeout = 200,
        };
        ble_gap_update_params(event->connect.conn_handle, &params);
    }
    return 0;
}

static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_hid_init(void) {
    printf("BLE: esp_hidd init\n");
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        printf("BLE: nimble_port_init error=%d\n", ret);
        return;
    }
    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, "Alpakka");
    if (ret != ESP_OK) {
        printf("BLE: adv_init error=%d\n", ret);
        return;
    }
    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_SIGN;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_SIGN;
    const esp_hid_device_config_t config = {
        .vendor_id = 0x1234,
        .product_id = 0x0001,
        .version = 0x0100,
        .device_name = "Alpakka",
        .manufacturer_name = "Input Labs",
        .serial_number = "000001",
        .report_maps = maps,
        .report_maps_len = 1,
    };
    ret = esp_hidd_dev_init(
        &config, ESP_HID_TRANSPORT_BLE, hid_event_handler, &hid_dev);
    if (ret != ESP_OK) {
        printf("BLE: esp_hidd_dev_init error=%d\n", ret);
        return;
    }
    printf("BLE: esp_hidd_dev_init ok\n");
    ble_gap_event_listener_register(&gap_listener,
                                    ble_gap_event_handler, NULL);
    nimble_port_freertos_init(ble_host_task);
}

// Stub required by esp_hid_gap.c (example uses it to start a demo task).
void ble_hid_task_start_up(void) {}

typedef struct __attribute__((packed)) {
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
    int16_t lz;
    int16_t rz;
    uint32_t buttons;
} rp2040_gamepad_t;

typedef struct __attribute__((packed)) {
    uint8_t buttons_0;
    uint8_t buttons_1;
    uint8_t lz;
    uint8_t rz;
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
    uint8_t reserved[6];
} rp2040_xinput_t;

typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rz;
    uint8_t rx;
    uint8_t ry;
} ble_gamepad_t;

static void conv_gamepad(uint8_t *src, uint8_t *dst) {
    rp2040_gamepad_t *rp = (rp2040_gamepad_t *)src;
    ble_gamepad_t *ble = (ble_gamepad_t *)dst;
    ble->buttons = rp->buttons;
    ble->x = rp->lx;
    ble->y = rp->ly;
    ble->z = rp->rx;
    ble->rz = rp->ry;
    ble->rx = (rp->lz + 32767) >> 8;
    ble->ry = (rp->rz + 32767) >> 8;
}

static void conv_xinput(uint8_t *src, uint8_t *dst) {
    rp2040_xinput_t *x = (rp2040_xinput_t *)src;
    ble_gamepad_t *b = (ble_gamepad_t *)dst;
    uint16_t b0 = x->buttons_0, b1 = x->buttons_1;
    b->buttons =
        ((b0 >> 0) & 1) << 10 |   // UP
        ((b0 >> 1) & 1) << 11 |   // DOWN
        ((b0 >> 2) & 1) << 8  |   // LEFT
        ((b0 >> 3) & 1) << 9  |   // RIGHT
        ((b0 >> 4) & 1) << 13 |   // START
        ((b0 >> 5) & 1) << 12 |   // SELECT
        ((b0 >> 6) & 1) << 6  |   // L3
        ((b0 >> 7) & 1) << 7  |   // R3
        ((b1 >> 0) & 1) << 4  |   // L1
        ((b1 >> 1) & 1) << 5  |   // R1
        ((b1 >> 2) & 1) << 14 |   // HOME
        ((b1 >> 4) & 1) << 0  |   // A
        ((b1 >> 5) & 1) << 1  |   // B
        ((b1 >> 6) & 1) << 2  |   // X
        ((b1 >> 7) & 1) << 3;     // Y
    b->x  = x->lx;
    b->y  = x->ly;
    b->z  = x->rx;
    b->rz = x->ry;
    b->rx = x->lz;
    b->ry = x->rz;
}

void ble_hid_send_hid(uint8_t report_id, uint8_t *data, uint8_t len) {
    if (!connected || !hid_dev) return;
    ble_gamepad_t buf;
    if (report_id == 3) {
        conv_gamepad(data, (uint8_t *)&buf);
        data = (uint8_t *)&buf;
        len = sizeof(ble_gamepad_t);
    }
    if (report_id == 4) {
        conv_xinput(data, (uint8_t *)&buf);
        data = (uint8_t *)&buf;
        len = sizeof(ble_gamepad_t);
        report_id = 3;
    }
    esp_err_t ret = esp_hidd_dev_input_set(hid_dev, 0, report_id, data, len);
    if (ret != ESP_OK) {
        printf("BLE: input_set error=%d\n", ret);
    }
}

void ble_hid_battery_set(uint8_t level) {
    if (!hid_dev) return;
    esp_hidd_dev_battery_set(hid_dev, level);
}

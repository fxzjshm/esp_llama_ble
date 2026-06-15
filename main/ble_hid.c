// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2025, Input Labs Oy.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
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
// Report ID 3: Gamepad (15 bytes: buttons[2], hat[1], x[2], y[2], z[2], rx[2], ry[2], rz[2])
// Standard Xbox/PS controller mapping on Linux:
// X=leftX, Y=leftY, Z=leftTrigger, Rx=rightX, Ry=rightY, Rz=rightTrigger
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
    0x09, 0x39,       //   Usage (Hat Switch)
    0x15, 0x01,       //   Logical Minimum (1)
    0x25, 0x08,       //   Logical Maximum (8)
    0x35, 0x00,       //   Physical Minimum (0)
    0x46, 0x3B, 0x01, //   Physical Maximum (315)
    0x65, 0x12,       //   Unit (SI Rot : Ang Pos)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x01,       //   Report Count (1)
    0x81, 0x42,       //   Input (Data,Var,Abs,Null)
    0x05, 0x01,       //   Usage Page (Generic Desktop)
    0x09, 0x30,       //   Usage (X)       — left stick X
    0x09, 0x31,       //   Usage (Y)       — left stick Y
    0x09, 0x32,       //   Usage (Z)       — left trigger
    0x09, 0x33,       //   Usage (Rx)      — right stick X
    0x09, 0x34,       //   Usage (Ry)      — right stick Y
    0x09, 0x35,       //   Usage (Rz)      — right trigger
    0x16, 0x00, 0x80, //   Logical Minimum (-32767)
    0x26, 0xFF, 0x7F, //   Logical Maximum (32767)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x06,       //   Report Count (6)
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
    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GAMEPAD, "Alpakka");
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
    uint8_t report_id;    // USB HID report ID (always 0)
    uint8_t report_size;  // always 20 (XINPUT_REPORT_SIZE)
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
    uint8_t hat;   // Hat Switch (0=centered, 1-8=directions)
    int16_t x;     // left stick X  (X usage)
    int16_t y;     // left stick Y  (Y usage)
    int16_t lz;    // left trigger  (Z usage)
    int16_t rx;    // right stick X (Rx usage)
    int16_t ry;    // right stick Y (Ry usage)
    int16_t rz;    // right trigger (Rz usage)
} ble_gamepad_t;

static uint8_t hat_from_dpad(bool up, bool down, bool left, bool right) {
    if      ( up && !down && !left && !right) return 1;
    else if ( up && !down && !left &&  right) return 2;
    else if (!up && !down && !left &&  right) return 3;
    else if (!up &&  down && !left &&  right) return 4;
    else if (!up &&  down && !left && !right) return 5;
    else if (!up &&  down &&  left && !right) return 6;
    else if (!up && !down &&  left && !right) return 7;
    else if ( up && !down &&  left && !right) return 8;
    return 0;
}

static void conv_gamepad(uint8_t *src, uint8_t *dst) {
    rp2040_gamepad_t *rp = (rp2040_gamepad_t *)src;
    ble_gamepad_t *b = (ble_gamepad_t *)dst;
    // Buttons: remap to match Linux standard gamepad mapping
    b->buttons =
        ((rp->buttons >> 0) & 1) << 0  |   // A        → bit 0  (BTN_SOUTH)
        ((rp->buttons >> 1) & 1) << 1  |   // B        → bit 1  (BTN_EAST)
        0                                 |   //          bit 2  (BTN_C, unused)
        ((rp->buttons >> 2) & 1) << 3  |   // X        → bit 3  (BTN_NORTH)
        ((rp->buttons >> 3) & 1) << 4  |   // Y        → bit 4  (BTN_WEST)
        0                                 |   //          bit 5  (BTN_Z, unused)
        ((rp->buttons >> 4) & 1) << 6  |   // L1       → bit 6  (BTN_TL)
        ((rp->buttons >> 5) & 1) << 7  |   // R1       → bit 7  (BTN_TR)
        0                                 |   //          bit 8  (BTN_TL2, unused)
        0                                 |   //          bit 9  (BTN_TR2, unused)
        ((rp->buttons >> 12) & 1) << 10 |  // SELECT   → bit 10 (BTN_SELECT)
        ((rp->buttons >> 13) & 1) << 11 |  // START    → bit 11 (BTN_START)
        ((rp->buttons >> 14) & 1) << 12 |  // HOME     → bit 12 (BTN_MODE)
        ((rp->buttons >> 6) & 1) << 13 |   // L3       → bit 13 (BTN_THUMBL)
        ((rp->buttons >> 7) & 1) << 14 |   // R3       → bit 14 (BTN_THUMBR)
        0;                                 //          bit 15 (unused)
    // Hat Switch: convert D-pad buttons to hat value
    b->hat = hat_from_dpad(
        (rp->buttons >> 10) & 1,  // UP
        (rp->buttons >> 11) & 1,  // DOWN
        (rp->buttons >> 8) & 1,   // LEFT
        (rp->buttons >> 9) & 1);  // RIGHT
    // Axes
    b->x  = rp->lx;   // X usage = left X
    b->y  = rp->ly;   // Y usage = left Y
    b->lz = rp->lz;   // Z usage = left trigger
    b->rx = rp->rx;   // Rx usage = right X
    b->ry = rp->ry;   // Ry usage = right Y
    b->rz = rp->rz;   // Rz usage = right trigger
}

static void conv_xinput(uint8_t *src, uint8_t *dst) {
    rp2040_xinput_t *x = (rp2040_xinput_t *)src;
    ble_gamepad_t *b = (ble_gamepad_t *)dst;
    uint16_t b0 = x->buttons_0, b1 = x->buttons_1;
    // Buttons: remap to match Linux standard gamepad mapping
    b->buttons =
        ((b1 >> 4) & 1) << 0  |   // A        → bit 0  (BTN_SOUTH)
        ((b1 >> 5) & 1) << 1  |   // B        → bit 1  (BTN_EAST)
        0                         |   //          bit 2  (BTN_C, unused)
        ((b1 >> 6) & 1) << 3  |   // X        → bit 3  (BTN_NORTH)
        ((b1 >> 7) & 1) << 4  |   // Y        → bit 4  (BTN_WEST)
        0                         |   //          bit 5  (BTN_Z, unused)
        ((b1 >> 0) & 1) << 6  |   // L1       → bit 6  (BTN_TL)
        ((b1 >> 1) & 1) << 7  |   // R1       → bit 7  (BTN_TR)
        0                         |   //          bit 8  (BTN_TL2, unused)
        0                         |   //          bit 9  (BTN_TR2, unused)
        ((b0 >> 5) & 1) << 10 |   // SELECT   → bit 10 (BTN_SELECT)
        ((b0 >> 4) & 1) << 11 |   // START    → bit 11 (BTN_START)
        ((b1 >> 2) & 1) << 12 |   // HOME     → bit 12 (BTN_MODE)
        ((b0 >> 6) & 1) << 13 |   // L3       → bit 13 (BTN_THUMBL)
        ((b0 >> 7) & 1) << 14 |   // R3       → bit 14 (BTN_THUMBR)
        0;                         //          bit 15 (unused)
    // Hat Switch: convert D-pad buttons to hat value
    b->hat = hat_from_dpad(
        (b0 >> 0) & 1,  // UP
        (b0 >> 1) & 1,  // DOWN
        (b0 >> 2) & 1,  // LEFT
        (b0 >> 3) & 1); // RIGHT
    // Axes
    b->x  = x->lx;       // X usage = left X
    b->y  = -x->ly;      // Y usage = left Y (undo XInput negation)
    b->lz = ((int16_t)x->lz * 257) - 32767;  // Z usage = left trigger [0,255]→[-32767,32767]
    b->rx = x->rx;       // Rx usage = right X
    b->ry = -x->ry;      // Ry usage = right Y (undo XInput negation)
    b->rz = ((int16_t)x->rz * 257) - 32767;  // Rz usage = right trigger [0,255]→[-32767,32767]
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

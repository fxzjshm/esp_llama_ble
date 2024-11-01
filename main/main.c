// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, Input Labs Oy.

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <esp_random.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_timer.h>

#define ESPNOW_CHANNEL 9
#define ESPNOW_PMK "pmk1234567890123"
#define ESPNOW_LMK "lmk1234567890123"

#define TASK_STACK 2048

#define UART_PRIMARY_PIN_TX 20
#define UART_PRIMARY_PIN_RX 19
#define UART_SECONDARY_PIN_TX 7
#define UART_SECONDARY_PIN_RX 6
#define UART_RATE 115200 * 8
#define UART_BUFFER_SIZE 1024

static uint8_t MAC_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// static uint8_t MAC_C6_DEVBOARD_A[6] = {0xF0, 0xF5, 0xBD, 0x05, 0xDB, 0xA8};
// static uint8_t MAC_C6_DEVBOARD_B[6] = {0xF0, 0xF5, 0xBD, 0x05, 0xCD, 0x2C};
// static uint8_t MAC_C2_DEVBOARD_A[6] = {0x08, 0x3a, 0x8d, 0x40, 0xd8, 0x00};
// static uint8_t MAC_C2_BREAKOUT_A[6] = {0x80, 0x64, 0x6f, 0x41, 0x02, 0x60};
// static uint8_t MAC_DONGLE[6];
// static uint8_t MAC_CONTROLLER[6];


void loguart(char *msg) {
    char data[32] = {0,};
    memcpy(data, msg, strlen(msg));
    uart_write_bytes(UART_NUM_1, (const char*)data, 32);
}

void print_array(uint8_t *array, uint8_t len, bool hex, bool newline) {
    printf("[");
    for(uint8_t i=0; i<len; i++) {
        if (hex) printf("0x%02x ", array[i]);
        else printf("%i ", array[i]);
    }
    printf("]");
    if (newline) printf("\n");
}

static void wifi_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

static void uart_init() {
    uart_config_t uart_config = {
        .baud_rate  = UART_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // Move uart0 (stdio) to secondary pins.
    ESP_ERROR_CHECK(uart_set_pin(
        UART_NUM_0,
        UART_SECONDARY_PIN_TX,
        UART_SECONDARY_PIN_RX,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));
    // Config uart1 (data) to primary pins.
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_NUM_1,
        UART_PRIMARY_PIN_TX,
        UART_PRIMARY_PIN_RX,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));
}

// static void dongle_task() {
//     while (true) {
//         vTaskDelay(1000);
//     }

//     // uint8_t *data = (uint8_t *)malloc(UART_MSG_LEN);
//     // while(true) {
//     //     uart_wait_tx_idle_polling(UART_NUM_1);
//     //     uint8_t data[UART_MSG_LEN] = {0,};
//     //     int64_t timestamp = esp_timer_get_time();
//     //     memcpy(data, &timestamp, 8);
//     //     data[15] = 255;
//     //     uint8_t len = uart_write_bytes(UART_NUM_1, (char*)data, UART_MSG_LEN);
//     //     if (len != UART_MSG_LEN) {
//     //         ESP_LOGI("SEND ERROR", "%i", len);
//     //     }
//     //     // ESP_LOGI("UART_TX", "Sent %i %i %i %i %i %i %i %i ", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
//     //     // ESP_LOGI("TICK", "%i %i %li %li", configTICK_RATE_HZ, SEND_RATE, portTICK_PERIOD_MS, SEND_RATE / portTICK_PERIOD_MS);
//     //     vTaskDelay(SEND_RATE / portTICK_PERIOD_MS);
//     // }
//     // free(data);
// }

// static void controller_task() {
//     // uint8_t* data = (uint8_t*)malloc(16);
//     uint8_t data[32] = {0,};


//     while(true) {
//         int64_t now = esp_timer_get_time();
//         memcpy(data, (uint8_t*)&now, 8);

//         uint8_t err = esp_now_send(MAC_DONGLE, data, 16);
//         if (err) printf("send error=%i\n", err);
//         printf("send ");
//         print_array(data, 8, false, true);
//         vTaskDelay(1000);

//         // size_t pending = 0;
//         // uart_get_buffered_data_len(UART_NUM_1, &pending);
//         // ESP_LOGI("PENDING", "%i", pending);

//         // int8_t len = uart_read_bytes(UART_NUM_1, data, UART_MSG_LEN, UART_TIMEOUT);
//         // if (len == UART_MSG_LEN) {

//         //     if (data[15] == 255) {
//         //         // ESP_LOGI(
//         //         //     "CONTROLLER",
//         //         //     "Combined (%i) %i %i %i %i %i %i %i %i", len,
//         //         //     data[0], data[1],  data[2],  data[3],  data[4],  data[5],  data[6],  data[7]
//         //         //     // data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]
//         //         // );

//         //         uint8_t err = esp_now_send(MAC_DONGLE, data, UART_MSG_LEN);
//         //         if (err) ESP_LOGI("ESPNOW_TX", "error=%i", err);
//         //     } else {
//         //         ESP_LOGE("CONTROLLER", "Termination bit %i", data[15]);
//         //     }
//         // } else if (len == 0) {
//         //     vTaskDelay(1 / portTICK_PERIOD_MS);
//         // } else {
//         //     ESP_LOGE("CONTROLLER", "UART error %i", len);
//         // }
//     }
// }

static void unified_task() {
    static uint8_t i = 0;
    static uint8_t payload[32] = {0,};
    while(true) {
        uint16_t pending = 0;
        uart_get_buffered_data_len(UART_NUM_1, (size_t*)&pending);
        if (pending == 0) {
            vTaskDelay(1);
            continue;
        }
        char buffer[1] = {0};
        uart_read_bytes(UART_NUM_1, &buffer, 1, 1);
        char c = buffer[0];
        // Check control bytes.
        if (i < 4) {
            if ((i==0 && c==16) || (i==1 && c==32) || (i==2 && c==64) || (i==3 && c==128))  {
                i += 1;
            } else {
                i = 0;
                // printf("UART misaligned\n");
            }
        } else {
            // Get payload.
            payload[i-4] = c;
            i += 1;
            // Payload complete.
            if (i == 32+4) {
                // print_array(payload, 32);
                uint8_t err = esp_now_send(MAC_BROADCAST, payload, 32);
                if (err) printf("send error=%i\n", err);
                i = 0;
            }
        }
    }
}

static void mock_task_1() {
    while(true) {
        char control[4] = {16, 32, 64, 128,};
        char payload[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
        uint8_t sent = 0;
        sent = uart_write_bytes(UART_NUM_1, control, 4);
        if (sent != 4) printf("UART write error\n");
        sent = uart_write_bytes(UART_NUM_1, payload, 32);
        if (sent != 32) printf("UART write error\n");
        printf("miau\n");
        vTaskDelay(2000);
    }
}

static void mock_task_2() {
    while(true) {
        uint8_t payload[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
        uint8_t err = esp_now_send(MAC_BROADCAST, payload, 32);
        printf("send error=%i\n", err);
        vTaskDelay(2000);
    }
}

static void unified_callback(
    const esp_now_recv_info_t *recv_info,
    const uint8_t *data,
    int len
) {
    char control[4] = {16, 32, 64, 128};
    // char message[32] = {0,};
    // memcpy(message, payload, 32);
    // memcpy(message, payload, 32);
    uint8_t sent;
    sent = uart_write_bytes(UART_NUM_1, control, 4);
    if (sent != 4) printf("UART write error\n");
    sent = uart_write_bytes(UART_NUM_1, data, len);
    if (sent != len) printf("UART write error\n");
}

static void mock_callback_1(
    const esp_now_recv_info_t *recv_info,
    const uint8_t *data,
    int len
) {
    char control[4] = {16, 32, 64, 128};
    // char message[32] = {0,};
    // memcpy(message, payload, 32);
    // memcpy(message, payload, 32);
    uint8_t sent;
    sent = uart_write_bytes(UART_NUM_1, control, 4);
    if (sent != 4) printf("UART write error\n");
    sent = uart_write_bytes(UART_NUM_1, data, len);
    if (sent != len) printf("UART write error\n");
}



// static void espnow_dongle_callback(
//     const esp_now_recv_info_t *recv_info,
//     const uint8_t *data,
//     int len
// ) {
//     char message[32] = "HID:";
//     memcpy(&message[4], data, 8);
//     uint8_t sent_len = uart_write_bytes(UART_NUM_1, message, 32);
//     if (sent_len != 32) printf("UART write error\n");
//     // printf("recv ");
//     // print_array(data, 8, false, true);
//     // esp_now_send(MAC_CONTROLLER, data, len);
// }

// static void espnow_controller_callback(
//     const esp_now_recv_info_t *recv_info,
//     const uint8_t *data,
//     int len
// ) {
//     static uint16_t iter = 0;
//     static float sum = 0;
//     static int64_t last_print = 0;
//     int64_t ts;
//     memcpy(&ts, data, 8);
//     int64_t now = esp_timer_get_time();
//     int64_t roundtrip = (now - ts);
//     sum += roundtrip;
//     iter++;
//     if (now - last_print > 250*1000) {
//         printf("roundtrip_avg=%.0f packets=%i\n", sum/iter, iter);
//         last_print = now;
//         iter = 0;
//         sum = 0;
//     }
// }

void get_mac(uint8_t* mac) {
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

bool compare_mac(uint8_t* mac1, uint8_t* mac2) {
    for (uint8_t i=0; i<6; i++) {
        if (mac1[i] != mac2[i]) return false;
    }
    return true;
}

void add_peer(uint8_t* mac) {
    printf("add_peer ");
    print_array(mac, 6, true, true);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    if (compare_mac(mac, MAC_BROADCAST)) peer.encrypt = false;
    else peer.encrypt = true;
    esp_err_t ret = esp_now_add_peer(&peer);
    ESP_ERROR_CHECK(ret);
}

void app_main(void) {
    // memcpy(MAC_CONTROLLER, MAC_C2_DEVBOARD_A, 6);
    // memcpy(MAC_DONGLE, MAC_C2_BREAKOUT_A, 6);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uart_init();
    wifi_init();
    esp_now_init();

    // uint8_t mac[6];
    // get_mac(mac);

    // // Dongle.
    // if (compare_mac(mac, MAC_DONGLE)) {
    //     printf("INIT dongle\n");
    //     add_peer(MAC_CONTROLLER);
    //     esp_now_register_recv_cb(espnow_dongle_callback);
    //     xTaskCreate(dongle_task, "dongle", TASK_STACK, NULL, 10, NULL);
    // }

    // // Controller.
    // if (compare_mac(mac, MAC_CONTROLLER)) {
    //     printf("INIT controller\n");
    //     add_peer(MAC_DONGLE);
    //     // esp_now_register_recv_cb(espnow_controller_callback);
    //     xTaskCreate(controller_task, "controller", TASK_STACK, NULL, 10, NULL);
    // }

    // Unified.
    printf("INIT unified\n");
    add_peer(MAC_BROADCAST);
    esp_now_register_recv_cb(unified_callback);
    xTaskCreate(unified_task, "task", TASK_STACK, NULL, 10, NULL);

    // xTaskCreate(mock_task_1, "task", TASK_STACK, NULL, 10, NULL);
    // esp_now_register_recv_cb(mock_callback_1);
}

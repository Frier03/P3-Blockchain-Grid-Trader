#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/sockets.h"
#include "esp_log.h"

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define WIFI_SSID      "BLOCKCHAIN"
#define WIFI_PASS      "10501115"
#define MAXIMUM_RETRY  5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

// The esp_log tag name, for this implementation file.
#define TAG_WIFI "LASET_WIFI"

static int s_retry_num = 0;

extern esp_ip4_addr_t node_ip;

// Functions
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void wifi_init_sta(void);

#endif
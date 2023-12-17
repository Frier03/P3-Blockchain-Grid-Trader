#include "wifi_connect.h"

// Used for handling WiFI events
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGV(TAG_WIFI, "Event happened!");
    ESP_LOGV(TAG_WIFI, "Event base : %s", event_base); // event_id is long int
    ESP_LOGV(TAG_WIFI, "Event ID   : %li", event_id); // event_id is long int

    // ----- 4. WIFI CONNECT PHASE
    // If WiFi started in station mode successfully, this event triggers
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    // ----- 6. WIFI DISCONNECT PHASE
    // If WiFi got disconnected, this event triggers
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_WIFI, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        // ESP_LOGI(TAG_WIFI,"connect to the AP fail");
    
    // ----- 5. WIFI GOT IP PHASE
    // When given IPv4 from DHCP server, this event triggers
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_retry_num = 0;

        // Set the node's ip.
        node_ip = event->ip_info.ip;  
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    // ----- 1. WIFI/LwIP INIT PHASE
    ESP_ERROR_CHECK(esp_netif_init());  // create an LwIP core task and initialize LwIP-related work
    ESP_ERROR_CHECK(esp_event_loop_create_default());   // create a system Event task and initialize an application event's callback function
    esp_netif_create_default_wifi_sta();    // create default network interface instance binding station with TCP/IP stack
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));   // create the Wi-Fi driver task and initialize the Wi-Fi driver.
    
    // ----- SETUP FOR (3. WIFI START PHASE)
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    // Make any ESP event get parsed by the <event_handler> method
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    
    // Make the "IP_EVENT_STA_GOT_IP" event get parsed by the <event_handler> method
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));


    // ----- 2. WIFI CONFIGURE PHASE
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) ); // to configure the Wi-Fi mode as station
    // You can call other esp_wifi_set_xxx APIs to configure more settings
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // ----- 3. WIFI START PHASE
    ESP_ERROR_CHECK(esp_wifi_start() ); // start the Wi-Fi driver
    ESP_LOGV(TAG_WIFI, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI, "Connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_WIFI, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG_WIFI, "UNEXPECTED EVENT");
    }
}
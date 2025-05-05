// --- Headers ---
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

// Include the new module headers
#include "wifi_manager.h"
#include "motor_control.h"
// web_server.h is implicitly included by wifi_manager.h which needs start/stop

// --- Application Configuration ---
#define APP_WIFI_SSID      "Glide-Resident" // Replace with actual SSID
#define APP_WIFI_PASS      "PokerLandPlant" // Replace with actual Password

static const char *TAG = "MAIN_APP";

// --- Main Application ---
void app_main(void)
{
    // Initialize NVS (needed for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta(APP_WIFI_SSID, APP_WIFI_PASS); // Pass credentials

    ESP_LOGI(TAG, "Initializing Motor Control...");
    motor_control_init(); // Initialize motors

    ESP_LOGI(TAG, "Initialization complete. Waiting for WiFi connection and MQTT commands...");

    // The main task can now idle or perform other background tasks.
    // WiFi connection, event handling, and the MQTT client run in their own tasks.
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); // Sleep for a minute
        ESP_LOGD(TAG, "Main task running..."); // Optional debug message
    }
} 
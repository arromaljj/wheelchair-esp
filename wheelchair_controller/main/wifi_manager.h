#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_event.h"

// --- Configuration ---
#define WIFI_MAXIMUM_RETRY  5

// --- Function Declarations ---

/**
 * @brief Initialize Wi-Fi in Station mode, connect to AP, and handle events.
 *        Starts the webserver upon successful connection.
 */
void wifi_init_sta(const char *ssid, const char *password);


#endif // WIFI_MANAGER_H 
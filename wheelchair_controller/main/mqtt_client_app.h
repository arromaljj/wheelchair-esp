#ifndef MQTT_CLIENT_APP_H
#define MQTT_CLIENT_APP_H

#include "esp_err.h"

/**
 * @brief Starts the MQTT client and connects to the broker.
 *
 * @return esp_err_t ESP_OK on success, or an error code otherwise.
 */
esp_err_t mqtt_app_start(void);

/**
 * @brief Stops and destroys the MQTT client.
 *
 * @return esp_err_t ESP_OK on success, or an error code otherwise.
 */
esp_err_t mqtt_app_stop(void);

#endif // MQTT_CLIENT_APP_H 
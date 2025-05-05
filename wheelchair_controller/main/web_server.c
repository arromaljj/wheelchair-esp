#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "web_server.h"      // Include the header for this module
#include "motor_control.h"   // Include motor control functions

static const char *TAG = "WEB_SERVER";

// --- Web Server Handlers ---

/* An HTTP GET handler */
static esp_err_t control_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char speed_str[10];
    char stop_str[5];

    // Get query string
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "Failed to allocate memory for query buffer");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);

            // Check for emergency stop parameter
            if (httpd_query_key_value(buf, "stop", stop_str, sizeof(stop_str)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => stop=%s", stop_str);
                if (atoi(stop_str) == 1) {
                     motor_emergency_stop();
                     httpd_resp_send(req, "Emergency Stop Activated", HTTPD_RESP_USE_STRLEN);
                     free(buf);
                     return ESP_OK;
                }
            }

            // Check for speed parameter
            if (httpd_query_key_value(buf, "speed", speed_str, sizeof(speed_str)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => speed=%s", speed_str);
                // Basic validation: check if it's a number (potentially with a sign)
                char *endptr;
                int speed = (int)strtol(speed_str, &endptr, 10);
                if (*endptr == '\0') { // Check if conversion was successful
                    motor_set_state(speed);
                    // Respond to the client
                    char resp_str[50];
                    snprintf(resp_str, sizeof(resp_str), "Motor speed set to %d%%", speed);
                    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
                    free(buf);
                    return ESP_OK;
                } else {
                     ESP_LOGW(TAG, "Invalid speed value: %s", speed_str);
                }
            }
        }
        free(buf);
    }

    // If no valid parameters found or error occurred
    ESP_LOGW(TAG, "Control request failed or parameters invalid.");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL); // Send 400 Bad Request
    return ESP_FAIL;
}

static const httpd_uri_t control_uri = {
    .uri       = "/control",
    .method    = HTTP_GET,
    .handler   = control_get_handler,
    .user_ctx  = NULL
};

// --- Web Server Start/Stop Implementation ---

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server_handle = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server_handle, &control_uri);
        // Add more handlers here if needed
        return server_handle;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server_handle)
{
    if (server_handle) {
        // Stop the httpd server
        ESP_LOGI(TAG, "Stopping web server.");
        httpd_stop(server_handle);
    }
} 
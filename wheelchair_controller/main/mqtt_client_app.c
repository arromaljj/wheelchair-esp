#include <stdio.h>
#include <string.h>
#include <stdlib.h>               // For strtol
#include "freertos/FreeRTOS.h"    // For task management
#include "freertos/task.h"        // For vTaskDelay, xTaskCreate
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_client_app.h"
#include "motor_control.h"
#include "esp_crt_bundle.h"       // esp_crt_bundle_attach()
#include "cJSON.h"                // For JSON parsing/creation

static const char *TAG = "MQTT_APP";

/* -------- Configuration (move to Kconfig later) -------------------------- */
#define MQTT_BROKER_URI         "mqtts://ceff3b2fc9074ac487a7ba2d62c24ef5.s1.eu.hivemq.cloud:8883"
#define MQTT_USERNAME           "wheelchair-esp32-1"
#define MQTT_PASSWORD           "Wheelchair-reach1010"
// New Topics
#define MQTT_STATE_TOPIC        "wheelchair/state"         // Topic for publishing state
#define MQTT_MOTOR_CMD_TOPIC    "wheelchair/command/motor" // Topic for receiving motor commands (JSON)
#define MQTT_EMERGENCY_CMD_TOPIC "wheelchair/command/emergency" // Topic for emergency STOP/START
#define STATE_PUBLISH_INTERVAL_MS 200 // Publish state every 200ms
/* ------------------------------------------------------------------------ */

static esp_mqtt_client_handle_t client = NULL;
static bool g_emergency_stopped = false; // Global flag for emergency stop state
static TaskHandle_t g_publish_task_handle = NULL; // Handle for the state publishing task

// --- Forward Declarations ---
static void publish_motor_state_task(void *pvParameters);
static void handle_motor_command(const char *data, int data_len);
static void handle_emergency_command(const char *data, int data_len);

static void log_error_if_nonzero(const char *msg, int err)
{
    if (err != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", msg, err);
    }
}

/* MQTT event handler ------------------------------------------------------ */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t c = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        g_emergency_stopped = false; // Reset emergency stop on reconnect

        // Subscribe to command topics
        msg_id = esp_mqtt_client_subscribe(c, MQTT_MOTOR_CMD_TOPIC, 1);
        ESP_LOGI(TAG, "Subscribed (msg_id=%d) to %s", msg_id, MQTT_MOTOR_CMD_TOPIC);
        msg_id = esp_mqtt_client_subscribe(c, MQTT_EMERGENCY_CMD_TOPIC, 1);
        ESP_LOGI(TAG, "Subscribed (msg_id=%d) to %s", msg_id, MQTT_EMERGENCY_CMD_TOPIC);

        // Start the state publishing task if it's not already running
        if (g_publish_task_handle == NULL) {
             xTaskCreate(publish_motor_state_task, "mqtt_pub_task", 4096, c, 5, &g_publish_task_handle);
             if(g_publish_task_handle == NULL) {
                 ESP_LOGE(TAG, "Failed to create state publishing task!");
             } else {
                 ESP_LOGI(TAG, "State publishing task started.");
             }
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        // Stop the state publishing task
        if (g_publish_task_handle != NULL) {
            ESP_LOGI(TAG, "Stopping state publishing task...");
            vTaskDelete(g_publish_task_handle);
            g_publish_task_handle = NULL;
            ESP_LOGI(TAG, "State publishing task stopped.");
        }
        g_emergency_stopped = true; // Enter safe state on disconnect
        motor_emergency_stop(); // Ensure motors are stopped
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        // Log only for debug, can be noisy
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGD(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGD(TAG, "DATA=%.*s", event->data_len, event->data);

        // Route data based on topic
        if (event->topic && strncmp(event->topic, MQTT_MOTOR_CMD_TOPIC, event->topic_len) == 0) {
            handle_motor_command(event->data, event->data_len);
        } else if (event->topic && strncmp(event->topic, MQTT_EMERGENCY_CMD_TOPIC, event->topic_len) == 0) {
            handle_emergency_command(event->data, event->data_len);
        } else {
            ESP_LOGW(TAG, "Received data on unexpected topic: %.*s", event->topic_len, event->topic);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("esp‑tls",
                                 event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("tls stack",
                                 event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("socket errno",
                                 event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "errno string (%s)",
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type ==
                   MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused: 0x%x",
                     event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x",
                     event->error_handle->error_type);
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id: %d", event->event_id);
        break;
    }
}

// --- Command Handlers ---

static void handle_motor_command(const char *data, int data_len) {
    if (g_emergency_stopped) {
        ESP_LOGW(TAG, "Motor command ignored - EMERGENCY STOP active.");
        return;
    }

    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse motor command JSON");
        return;
    }

    const cJSON *left_json = cJSON_GetObjectItemCaseSensitive(root, "left");
    const cJSON *right_json = cJSON_GetObjectItemCaseSensitive(root, "right");

    if (!cJSON_IsNumber(left_json) || !cJSON_IsNumber(right_json)) {
        ESP_LOGE(TAG, "Invalid motor command JSON format: 'left' and 'right' must be numbers.");
        cJSON_Delete(root);
        return;
    }

    int left_speed = left_json->valueint;
    int right_speed = right_json->valueint;

    ESP_LOGI(TAG, "Received motor command: Left=%d, Right=%d", left_speed, right_speed);

    // Validate speed range (optional, motor_control might clamp anyway)
    if (left_speed < -100 || left_speed > 100 || right_speed < -100 || right_speed > 100) {
         ESP_LOGW(TAG, "Motor command speed out of range (-100 to 100). Clamping may occur.");
    }

    motor_set_speeds(left_speed, right_speed);

    cJSON_Delete(root); // Free JSON object
}

static void handle_emergency_command(const char *data, int data_len) {
    char command[data_len + 1];
    memcpy(command, data, data_len);
    command[data_len] = '\0';

    if (strcmp(command, "STOP") == 0) {
        if (!g_emergency_stopped) {
            ESP_LOGW(TAG, "EMERGENCY STOP command received.");
            g_emergency_stopped = true;
            motor_emergency_stop();
        } else {
            ESP_LOGW(TAG, "Emergency stop already active.");
        }
    } else if (strcmp(command, "START") == 0) {
        if (g_emergency_stopped) {
            ESP_LOGW(TAG, "MOTOR START command received.");
            g_emergency_stopped = false;
            // Optionally set speed to 0 as a safe default, or let next motor command decide
            // motor_set_speeds(0, 0);
            ESP_LOGI(TAG, "Motors enabled. Awaiting motor commands.");
        } else {
            ESP_LOGW(TAG, "Motors already enabled.");
        }
    } else {
        ESP_LOGW(TAG, "Invalid emergency command: %s. Use 'STOP' or 'START'.", command);
    }
}


// --- State Publishing Task ---
static void publish_motor_state_task(void *pvParameters) {
    esp_mqtt_client_handle_t mqtt_client = (esp_mqtt_client_handle_t)pvParameters;
    cJSON *root = NULL;
    char *json_string = NULL;

    ESP_LOGI(TAG, "State publisher task started.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(STATE_PUBLISH_INTERVAL_MS));

        if (!mqtt_client || g_emergency_stopped) { // Don't publish if client is gone or emergency stopped
             if (g_emergency_stopped) ESP_LOGD(TAG, "State publish skipped (Emergency Stop)");
             continue;
        }


        int left_speed, right_speed;
        motor_get_speeds(&left_speed, &right_speed);

        // Create JSON payload
        root = cJSON_CreateObject();
        if (root == NULL) {
            ESP_LOGE(TAG, "Failed to create JSON object for state");
            continue; // Skip this iteration
        }
        cJSON_AddNumberToObject(root, "left_speed", left_speed);
        cJSON_AddNumberToObject(root, "right_speed", right_speed);

        json_string = cJSON_PrintUnformatted(root); // Use Unformatted for smaller size
        if (json_string == NULL) {
            ESP_LOGE(TAG, "Failed to print JSON state to string");
            cJSON_Delete(root);
            continue;
        }

        // Publish the state
        int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_STATE_TOPIC, json_string, 0, 0, 0); // QoS 0
        if (msg_id != -1) {
             ESP_LOGD(TAG, "Sent publish successful, topic=%s, msg_id=%d", MQTT_STATE_TOPIC, msg_id);
             ESP_LOGV(TAG, "Published state: %s", json_string); // Verbose log
        } else {
             ESP_LOGE(TAG, "Error sending publish, topic=%s", MQTT_STATE_TOPIC);
        }

        // Clean up
        cJSON_Delete(root);
        free(json_string);
        root = NULL;
        json_string = NULL;
    }

     // Should not reach here if vTaskDelete is used externally
     ESP_LOGW(TAG, "State publisher task exiting loop.");
     vTaskDelete(NULL);
}


/* Start / stop helpers ---------------------------------------------------- */
esp_err_t mqtt_app_start(void)
{
    ESP_LOGI(TAG, "Starting MQTT client...");

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URI,
            .verification.crt_bundle_attach = esp_crt_bundle_attach, /* key line */
        },
        .credentials = {
            .username = MQTT_USERNAME,
            .authentication.password = MQTT_PASSWORD,
        },
        // Optional: Set last will and testament (LWT) to indicate unexpected disconnect
        // .session.last_will = {
        //     .topic = "wheelchair/status",
        //     .msg = "offline",
        //     .qos = 1,
        //     .retain = 1
        // }
    };

    if (client) {
        ESP_LOGW(TAG, "Client already exists. Restarting it.");
        // Ensure clean stop before re-init
        mqtt_app_stop(); // mqtt_app_stop will also delete the task if running
        vTaskDelay(pdMS_TO_TICKS(200)); // Allow time for cleanup
    }


    client = esp_mqtt_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init() failed");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register event handler failed: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(client);
        client = NULL;
        return ret;
    }

    ret = esp_mqtt_client_start(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start() failed: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(client); // Cleanup client if start fails
        client = NULL;
    } else {
        ESP_LOGI(TAG, "MQTT client started successfully.");
    }
    return ret;
}

esp_err_t mqtt_app_stop(void)
{
    esp_err_t err = ESP_OK;

    // Stop and delete the publishing task first
    if (g_publish_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping state publishing task before stopping client...");
        vTaskDelete(g_publish_task_handle);
        g_publish_task_handle = NULL;
        ESP_LOGI(TAG, "State publishing task stopped.");
         // Add a small delay to allow the task deletion to complete
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (client) {
        ESP_LOGI(TAG, "Stopping MQTT client...");
        // Unregister event handler *before* stopping/destroying
        esp_mqtt_client_unregister_event(client, ESP_EVENT_ANY_ID,
                                         mqtt_event_handler);
        err = esp_mqtt_client_stop(client);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "esp_mqtt_client_stop failed: %s", esp_err_to_name(err));
        else
             ESP_LOGI(TAG, "MQTT client stopped.");


        // Destroy should be called after stop
        err = esp_mqtt_client_destroy(client);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "esp_mqtt_client_destroy failed: %s", esp_err_to_name(err));
        else
            ESP_LOGI(TAG, "MQTT client destroyed.");


        client = NULL;
        g_emergency_stopped = true; // Ensure safe state after stop
        motor_emergency_stop(); // Ensure motors are stopped
    } else {
        ESP_LOGI(TAG, "MQTT client already stopped/null.");
    }
    return err;
}

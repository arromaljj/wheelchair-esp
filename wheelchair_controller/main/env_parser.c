#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "env_parser.h"

#define MAX_ENV_VARS 10
#define MAX_KEY_LEN 32
#define MAX_VAL_LEN 64

static const char *TAG = "ENV_PARSER";

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VAL_LEN];
} env_var_t;

static env_var_t env_vars[MAX_ENV_VARS];
static int env_var_count = 0;

esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}


void parse_env_file(void) {
    if (init_spiffs() != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize SPIFFS. Cannot read .env file.");
        return;
    }

    FILE *f = fopen("/spiffs/.env", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open .env file from /spiffs/.env");
        // We need to copy the .env file to the spiffs partition image.
        // This is a manual step for the user.
        ESP_LOGE(TAG, "Please ensure the .env file is present in the spiffs partition image.");
        return;
    }

    char line[MAX_KEY_LEN + MAX_VAL_LEN + 2];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");

        if (key && value) {
            // Remove quotes from value if present
            if (value[0] == '"' && value[strlen(value) - 1] == '"') {
                value[strlen(value) - 1] = '\0';
                value++;
            }

            if (env_var_count < MAX_ENV_VARS) {
                strncpy(env_vars[env_var_count].key, key, MAX_KEY_LEN - 1);
                env_vars[env_var_count].key[MAX_KEY_LEN - 1] = '\0';
                strncpy(env_vars[env_var_count].value, value, MAX_VAL_LEN - 1);
                env_vars[env_var_count].value[MAX_VAL_LEN - 1] = '\0';
                env_var_count++;
            } else {
                ESP_LOGW(TAG, "Maximum number of environment variables reached");
                break;
            }
        }
    }

    fclose(f);
}

const char *get_env_value(const char *key) {
    for (int i = 0; i < env_var_count; i++) {
        if (strcmp(env_vars[i].key, key) == 0) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

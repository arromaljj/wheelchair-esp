idf_component_register(SRCS "main.c"
                         "wifi_manager.c"
                         "motor_control.c"
                         "mqtt_client_app.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi esp_event nvs_flash lwip mqtt json  
                    )

#!/bin/bash
# This script removes hardcoded credentials from the project files.

# Clean wheelchair-web-controller/script.js
if [ -f "wheelchair-web-controller/script.js" ]; then
  sed -i.bak 's/const MQTT_USERNAME = .*/const MQTT_USERNAME = "REMOVED_USERNAME";/' "wheelchair-web-controller/script.js"
  sed -i.bak 's/const MQTT_PASSWORD = .*/const MQTT_PASSWORD = "REMOVED_PASSWORD";/' "wheelchair-web-controller/script.js"
fi

# Clean wheelchair_controller/main/main.c
if [ -f "wheelchair_controller/main/main.c" ]; then
  sed -i.bak 's/#define APP_WIFI_PASS      .*/#define APP_WIFI_PASS      "REMOVED_WIFI_PASS"/' "wheelchair_controller/main/main.c"
fi

# Clean wheelchair_controller/main/mqtt_client_app.c
if [ -f "wheelchair_controller/main/mqtt_client_app.c" ]; then
  sed -i.bak 's/#define MQTT_USERNAME           .*/#define MQTT_USERNAME           "REMOVED_MQTT_USERNAME"/' "wheelchair_controller/main/mqtt_client_app.c"
  sed -i.bak 's/#define MQTT_PASSWORD           .*/#define MQTT_PASSWORD           "REMOVED_MQTT_PASSWORD"/' "wheelchair_controller/main/mqtt_client_app.c"
fi

# Remove backup files created by sed
find . -name "*.bak" -delete

# Project Setup and Credentials

This document provides instructions on how to configure your local environment with the necessary credentials to run the wheelchair controller and the web interface.

**Do not commit the credential files (`.env` and `config.js`) to version control.** They are already listed in `.gitignore` to prevent accidental exposure.

## 1. ESP32 Controller (`wheelchair_controller`)

The ESP32 controller requires a `.env` file to store your Wi-Fi and MQTT credentials. This file is loaded from a SPIFFS (SPI Flash File System) partition on the device.

### Steps:

1.  **Create a `spiffs` directory** inside the `wheelchair_controller` directory.

    ```bash
    mkdir -p wheelchair_controller/spiffs
    ```

2.  **Create a `.env` file** inside the newly created `spiffs` directory:

    `wheelchair_controller/spiffs/.env`

3.  **Add your credentials** to the `.env` file using the following format:

    ```
    WIFI_SSID="YOUR_WIFI_SSID"
    WIFI_PASS="YOUR_WIFI_PASSWORD"
    MQTT_USERNAME="YOUR_MQTT_USERNAME"
    MQTT_PASSWORD="YOUR_MQTT_PASSWORD"
    ```

    Replace the placeholder values with your actual credentials.

4.  **Build and Flash:** When you build and flash the project using `idf.py build flash`, the build system will automatically create a SPIFFS partition image from the `spiffs` directory and flash it to the device.

## 2. Web Interface (`wheelchair-web-controller`)

The web interface requires a `config.js` file to provide the MQTT credentials to the browser application.

### Steps:

1.  **Create a `config.js` file** inside the `wheelchair-web-controller` directory.

2.  **Add your MQTT credentials** to the `config.js` file using the following format:

    ```javascript
    const MQTT_USERNAME = 'YOUR_MQTT_USERNAME';
    const MQTT_PASSWORD = 'YOUR_MQTT_PASSWORD';
    ```

    Replace the placeholder values with your actual credentials. Note that these should be the same credentials used in the `.env` file for the ESP32 controller.

After completing these steps, you should be able to run both the ESP32 controller and the web interface successfully.

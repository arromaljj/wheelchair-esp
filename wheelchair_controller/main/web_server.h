#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"

// --- Function Declarations ---

/**
 * @brief Start the web server and register URI handlers.
 *
 * @return httpd_handle_t Handle to the started server, or NULL on failure.
 */
httpd_handle_t start_webserver(void);

/**
 * @brief Stop the web server.
 *
 * @param server Handle of the server to stop.
 */
void stop_webserver(httpd_handle_t server);

#endif // WEB_SERVER_H 
/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
////////////////////////////////////////////////////////////////////////////////////////////////
#include "esp_system.h"

#include "esp_err.h"
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>

#include "driver/gpio.h"

static const char *TAG = "webserver";

#define LED GPIO_NUM_2

/******************  WEBSEVER CODE BEGINS ***********************/

static esp_err_t ledOFF_handler(httpd_req_t *req)
{
	esp_err_t error;
	ESP_LOGI(TAG, "LED Turned OFF");
	gpio_set_level(LED, 0);
	const char *response = (const char *) req->user_ctx;
	error = httpd_resp_send(req, response, strlen(response));
	if (error != ESP_OK)
	{
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	}
	else ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static const httpd_uri_t ledoff = {
    .uri       = "/ledoff",
    .method    = HTTP_GET,
    .handler   = ledOFF_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "<!DOCTYPE html>\
<html>\
<head>\
<style>\
.button {\
  border: none;\
  color: white;\
  padding: 15px 32px;\
  text-align: center;\
  text-decoration: none;\
  display: inline-block;\
  font-size: 16px;\
  margin: 4px 2px;\
  cursor: pointer;\
}\
\
.button1 {background-color: #4CAF50;} /* Green */\
</style>\
</head>\
<body>\
\
<h1>ESP32 WEBSERVER</h1>\
<p>Toggle the onboard LED (GPIO2)</p>\
<h3> LED STATE : OFF </h3>\
\
<button class=\"button button1\" onclick= \"window.location.href='/ledon'\">LED ON</button>\
\
</body>\
</html>"
};


static esp_err_t ledON_handler(httpd_req_t *req)
{
	esp_err_t error;
	ESP_LOGI(TAG, "LED Turned ON");
	gpio_set_level(LED, 1);
	const char *response = (const char *) req->user_ctx;
	error = httpd_resp_send(req, response, strlen(response));
	if (error != ESP_OK)
	{
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	}
	else ESP_LOGI(TAG, "Response sent Successfully");
	return error;
}

static const httpd_uri_t ledon = {
    .uri       = "/ledon",
    .method    = HTTP_GET,
    .handler   = ledON_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "<!DOCTYPE html>\
<html>\
<head>\
<style>\
.button {\
  border: none;\
  color: white;\
  padding: 15px 32px;\
  text-align: center;\
  text-decoration: none;\
  display: inline-block;\
  font-size: 16px;\
  margin: 4px 2px;\
  cursor: pointer;\
}\
\
.button1 {background-color: #000000;} /* Green */\
</style>\
</head>\
<body>\
\
<h1>ESP32 WEBSERVER</h1>\
<p>Toggle the onboard LED (GPIO2)</p>\
<h3> LED STATE : ON </h3>\
\
<button class=\"button button1\" onclick= \"window.location.href='/ledoff'\">LED OFF</button>\
\
</body>\
</html>"
};

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = ledOFF_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "<!DOCTYPE html>\
<html>\
<head>\
<style>\
.button {\
  border: none;\
  color: white;\
  padding: 15px 32px;\
  text-align: center;\
  text-decoration: none;\
  display: inline-block;\
  font-size: 16px;\
  margin: 4px 2px;\
  cursor: pointer;\
}\
\
.button1 {background-color: #4CAF50;} /* Green */\
</style>\
</head>\
<body>\
\
<h1>ESP32 WEBSERVER</h1>\
<p>Toggle the onboard LED (GPIO2)</p>\
<h3> LED STATE : OFF </h3>\
\
<button class=\"button button1\" onclick= \"window.location.href='/ledon'\">LED ON</button>\
\
</body>\
</html>"
};


esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ledoff);
        httpd_register_uri_handler(server, &ledon);
        httpd_register_uri_handler(server, &root);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

/******************* WEBSERVER CODE ENDS ************************/

static void configure_led(void)
{
	gpio_reset_pin (LED);

	gpio_set_direction (LED, GPIO_MODE_OUTPUT);
}


/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN



static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    /* Initialize the underlying TCP/IP stack and check for errors, stops the program if returned
       value is no ESP_OK */
    ESP_ERROR_CHECK(esp_netif_init());
    /* Creates a default event loop. Loop library allows components to declare events so that
       other components can register handlers (functions that executes when those events happens).
       An event indicates an important occurrance such as a WiFi successful connection to an
       access point. The event loop is the bridge between events and event handlers. The default
       event loop is a special type of loop used for system events such as WiFi events. */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* This API creates an esp_netif object with the default WiFi access point configuration,
       attaches the netif object to WiFi access point and registers WiFi AP event handlers to the
       default event loop. This API uses assert() to check for potential errors, so it could abbort
       the program (Note that the default event loop needs to be created prior to calling this API) */
    esp_netif_create_default_wifi_ap();

    /* Declares the WiFi stack configuration parameters structure and initializes it with default
       values via WIFI_INIT_CONFIG_DEFAULT macro as a step to be passed to the esp_wifi_init
       function call */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* Initializes the WiFi, allocates resources for WiFi drivers such as WiFi control structure,
       RX/TX buffer, WiFi NVS structure, etc. Also starts WiFi task. This API must be called
       before all other WiFi APIs must be called. You must always use WIFI_INIT_CONFIG_DEFAULT to
       set the configuration default values, this guarantees that all the fields get the right
       value when more fields are added to wifi_init_config_t structure in a future release. If
       you want to set your own initial values, overwrite the default values which are setted by
       WIFI_INIT_CONFIG_DEFAULT. Please be notified that the field 'magic' of wifi_init_config_t
       should always be WIFI_INIT_CONFIG_MAGIC! (THIS IS WEIRD, DIG INTO THIS IN THE FUTURE),
       and check for errors */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Registers an instance of the WiFi event handler (wifi_event_handler) to the default loop,
       when any WiFi event happens, the wifi_event_handler handler function is called. Calling 
       this function with instance (last argument) set to NULL is equivalent to calling 
       esp_event_handler_register. So, in the next line, we have that case. */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    /* Soft-AP configuration settings for the device. */
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN, // Maximum stations number allowed to connect in.
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    /* If there is no password, configure the access point as open (without password). */
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{

	// HTTP server instance handler (a void pointer)
    static httpd_handle_t server = NULL;

	// Calls a function to configure the GPIO pin for the LED
    configure_led();
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
//    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
}

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*//*
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "wifi softAP";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else // CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}
*/
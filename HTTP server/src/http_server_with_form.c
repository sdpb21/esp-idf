#include "wifi.h"
#include "esp_http_server.h"
#include "esp32_spiffs.h"
#include "led_strip.h"

#define GPIO_CHANNEL	1
#define WS2812B_GPIO	5
#define REFRESH_TIMEOUT 100 // 100ms

#define SSID			"SSID"
#define PASSWORD		"jnrn5941"

esp_err_t		get_handler(httpd_req_t *req);
esp_err_t		submit_handler_function(httpd_req_t *req);
esp_err_t		uri_favicon(httpd_req_t *req);

httpd_handle_t	start_webserver(void);

led_strip_t *ws2812b;

void check_wifi_status(void *pvParam);

/* URI handler structure for GET / */
httpd_uri_t default_get = {
	.uri		= "/",
	.method		= HTTP_GET,
	.handler	= get_handler,
	.user_ctx	= NULL
};

/* URI handler structure for GET /submit */
httpd_uri_t submit_handler = {
	.uri		= "/submit",
	.method		= HTTP_GET,
	.handler	= submit_handler_function,
	.user_ctx	= NULL
};

// URI handler structure for GET /favicon.ico as the default uri when submitting the form
httpd_uri_t favicon = {
	.uri		= "/favicon.ico",
	.method		= HTTP_GET,
	.handler	= uri_favicon,
	.user_ctx	= NULL
};

void app_main(void) {
	/* LED strip initialization with the GPIO and pixels number*/
	ws2812b = led_strip_init(GPIO_CHANNEL, WS2812B_GPIO, 1);

	/* Set all LED off to clear all pixels */
	ws2812b->clear(ws2812b, REFRESH_TIMEOUT); // Can comment out this line

	spiffs_init();

	wifi_init();
	wifi_set_mode("STA");
	wifi_sta_begin(SSID, PASSWORD);
	start_webserver();
	xTaskCreate(check_wifi_status, "Check WiFi status", 2048, NULL, 1, NULL);
}

esp_err_t get_handler(httpd_req_t *req) {
	char *read_test_html;
	long file_size = get_file_size("test.html");

	if (file_size) { // If file existed, then read that file
		read_test_html = read_file("test.html", file_size + 1);
		httpd_resp_set_type(req, "text/html"); // Return HTML
		httpd_resp_send(req, read_test_html, strlen(read_test_html));

		printf("%s\n", read_test_html);

		free(read_test_html);
	} else {
		printf("File %s not existed\n", "test.html");
		httpd_resp_set_type(req, "text/html"); // Return HTML
		httpd_resp_send(req, "Can't load page, please load again", 100);
	}
	return ESP_OK;
}

esp_err_t submit_handler_function(httpd_req_t *req) {
	char query_string[100];
	char ssid[20], password[20];

	httpd_req_get_url_query_str(req, query_string, 100);
	printf("the whole query string: %s\n", query_string);

	httpd_query_key_value(query_string, "ssid", ssid, 20);
	httpd_query_key_value(query_string, "password", password, 20);

	// Replace + with space as RFC query_string replace space with +
	for (int i = 0; i < sizeof(ssid); i++) {
		if (ssid[i] == '+') ssid[i] = ' ';
	}

	for (int i = 0; i < sizeof(password); i++) {
		if (password[i] == '+') password[i] = ' ';
	}

	printf("SSID: %s\n", ssid);
	printf("PASSWORD: %s\n", password);

	httpd_resp_set_type(req, "text/html"); // Return text/html
	httpd_resp_send(req, "Config done", sizeof("Config done"));

	return ESP_OK;
}

// uri_favicon for /favicon.ico in form GET request
esp_err_t uri_favicon(httpd_req_t *req) {
	return ESP_OK;
}

/* Function for starting the webserver */
httpd_handle_t start_webserver(void) {
	/* Generate default configuration */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	/* Empty handle to esp_http_server */
	httpd_handle_t server = NULL;

	/* Start the httpd server */
	if (httpd_start(&server, &config) == ESP_OK) {
		/* Register URI handlers */
		httpd_register_uri_handler(server, &default_get);
		httpd_register_uri_handler(server, &submit_handler);
		httpd_register_uri_handler(server, &favicon);
	}
	/* If server failed to start, handle will be NULL */

	return server;
}

void check_wifi_status(void *pvParam) {
	while (1) {
		if (is_wifi_connected()) {
			ws2812b->set_pixel(ws2812b, 0, 0, 0, 16);
			ws2812b->refresh(ws2812b, REFRESH_TIMEOUT);
		} else {
			ws2812b->set_pixel(ws2812b, 0, 16, 0, 0);
			ws2812b->refresh(ws2812b, REFRESH_TIMEOUT);
			wifi_connect();
		}
		vTaskDelay(10/portTICK_PERIOD_MS);
	}
}
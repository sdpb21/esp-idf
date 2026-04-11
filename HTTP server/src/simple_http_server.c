#include "wifi.h"
#include "esp_http_server.h"

#define SSID		"SSID"
#define PASSWORD	"jnrn5941"

extern "C" {
	void app_main();
}

/* Our URI handler function to be called during GET / request */
esp_err_t get_handler(httpd_req_t *req) {
	/* Send a simple response */
	const char resp[] = "URI GET Response";
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

/* Our URI handler function to be called during GET /uri request */
esp_err_t uri_get_handler(httpd_req_t *req) {
	/* Send a simple response */
	const char resp[] = "Hello, World !";
	cout << "A HTTP client has access /uri\n";
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

/* uri_favicon for /favicon.ico in form GET request */
esp_err_t uri_favicon(httpd_req_t *req) {
	return ESP_OK;
};

/* URI handler structure for GET / */
httpd_uri_t default_get = {
	.uri		= "/",
	.method		= HTTP_GET,
	.handler	= get_handler,
	.user_ctx	= NULL
};

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
	.uri		= "/uri",
	.method		= HTTP_GET,
	.handler	= uri_get_handler,
	.user_ctx	= NULL
};

/* URI handler structure for GET /favicon.ico as the default uri */
httpd_uri_t favicon = {
	.uri		= "/favicon.ico",
	.method		= HTTP_GET,
	.handler	= uri_favicon,
	.user_ctx	= NULL
};

httpd_handle_t start_webserver(void) {
	/* Generate default configuration */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	/* Empty handle to esp_http_server */
	httpd_handle_t server = NULL;

	/* Start the httpd server */
	if (httpd_start(&server, &config) == ESP_OK) {
		/* Register URI handlers */
		httpd_register_uri_handler(server, &default_get);
		httpd_register_uri_handler(server, &uri_get);
		httpd_register_uri_handler(server, &favicon);
	}
	/* If server failed to start, handle will be NULL */
	return server;
}

void stop_webserver(httpd_handle_t server) {
	if (server) {
		httpd_stop(server);
	}
}

void app_main(void) {
	WiFiClass WiFi(WIFI_MODE_STA);
	WiFi.begin(SSID, PASSWORD);

	while (WiFi.sta_status() != WIFI_STA_CONNECTED) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
		cout << ".";
	}
	start_webserver();
}
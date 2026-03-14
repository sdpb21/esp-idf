#include "wifi.h"
#include "esp_http_server.h"
#include "esp32_spiffs.h"

#define SSID		"SSID"
#define PASSWORD	"PASSWORD"

esp_err_t		get_handler(httpd_req_t *req);
esp_err_t		uri_favicon(httpd_req_t *req);

httpd_handle_t 	start_webserver(void);

/* URI handler structure for GET / */
httpd_uri_t default_get = {
	.uri		= "/",
	.method		= HTTP_GET,
	.handler	= get_handler,
	.user_ctx	= NULL
};

// URI handler structure for GET /favicon.ico as the default uri when submitting the form
httpd_uri_t favicon = {
    .uri      = "/favicon.ico",
    .method   = HTTP_GET,
    .handler  = uri_favicon,
    .user_ctx = NULL
};

void app_main(void) {
	spiffs_init();

	wifi_init();
	wifi_set_mode("STA");
	wifi_sta_begin(SSID, PASSWORD);
	start_webserver();
}

esp_err_t get_handler(httpd_req_t *req) {
	char *read_test_html;
	long file_size = get_file_size("index.html");

	if (file_size) { // If file existed, then read that file
		read_test_html = read_file("index.html", file_size + 1);
		httpd_resp_set_type(req, "text/html"); // Return HTML
		httpd_resp_send(req, read_test_html, strlen(read_test_html));

		printf("%s\n", read_test_html);

		free(read_test_html);
	} else {
		printf("File %s not existed\n", "index.html");
		httpd_resp_set_type(req, "text/html"); // Return HTML
		httpd_resp_send(req, "Can't load page, please load again", 100);
	}
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
		httpd_register_uri_handler(server, &favicon);
	}
	/* If server failed to start, handle will be NULL */
	return server;
}
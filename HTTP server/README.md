# HTTP server built from TCP socket API to return an HTML page
Feature:
* A single-threaded event-driven HTTP server which uses select() to monitor a maxinum numbers of connected HTTP client
* HTML page stored in SPIFFS memory

Program:
* [http_server_select.cpp](src/http_server_select.cpp): This example is intended for comprehensive and easy to understand the single-thread HTTP server with select.
* Library [select_http_server](select_http_server): This library is intended for functions inheritance for other programs.
* [http_server_select_querystring.cpp](src/http_server_select_querystring.cpp): Parse querystring from a login form from library ``select_http_server``.

# A simple HTTP server with esp-idf built-in HTTP server library

The ESP-IDF built-in HTTP server library is a single-threaded event-driven server which uses select() to monitor multiple connected HTTP client.

A simple HTTP server on default port 80 that support GET request on ``/`` and ``/uri``

Program: [simple_http_server.cpp](src/simple_http_server.c)

``httpd_resp_send()`` will send the whole HTTP response one time. To send one HTTP chunk, use ``httpd_resp_send_chunk()``:

```c
esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len);
```

**Example**

```c
esp_err_t get_handler(httpd_req_t *req) {
	/* Send a simple response */
	const char resp[] = "URI GET Response";
	httpd_resp_send_chunk(req, resp, sizeof(resp));
	httpd_resp_send_chunk(req, resp, sizeof(resp));
	httpd_resp_send_chunk(req, resp, sizeof(resp));
	return ESP_OK;
}
```

# Query string handler

To get the whole query string then parse the value of field ``text`` of the URI

```cpp
esp_err_t get_handler(httpd_req_t *req) {
	char queryString[100];
	char text[20];
	/* Send a simple response */
	httpd_resp_send(req, configPortal, HTTPD_RESP_USE_STRLEN);
	httpd_req_get_url_query_str(req, queryString, 100);
	printf("the whole query string: %s\n", queryString);

	httpd_query_key_value(queryString, "text", text, 20);
	printf("text: %s\n", text);
	return ESP_OK;
}
```

# HTTP server to get data from a submitted form

A HTTP server gets the entered data from a form by parsing the query string.

Program: [http_server_with_form.c](src/http_server_with_form.c)
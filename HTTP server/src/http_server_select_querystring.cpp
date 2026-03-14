#include <iostream>
#include <regex>
#include "http_server.h"
#include "esp32_spiffs.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PORT		8000
#define SSID		"SSID"
#define PASSWORD	"jnrn5941"

extern "C" {
	void app_main();
}

vector<string> split_string_by_delim(string s, string delim);
void request_handler(string &request, string &response);
void http_server_task(void *pvParam);

void app_main() {
	WiFiClass WiFi(WIFI_MODE_STA);
	WiFi.begin(SSID, PASSWORD);

	while (WiFi.status() != WIFI_CONNECTED) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
		printf(".");
	}
	cout << "\nWiFi connected\n";

	spiffs_init(); 

	// Size of task must be big to host the server and store the http_server library
	xTaskCreate(http_server_task, "HTTP server task", 8192, NULL, 1, NULL);
}

void http_server_task(void *pvParam) {
	string request, response;
	HTTP_Server http_server(PORT, request_handler, request, response);
	http_server.start_server();

	while (1) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}
vector<string> split_string_by_delim(string s, string delim) {
	vector<string> all_substr;
	std::size_t index = s.find(delim, 0);
	string sub_str  = s.substr(0, index);
	string new_string = s.substr(index+1);

	while (index != string::npos) { 
		if (sub_str != delim && sub_str.size() >= 1) {
			all_substr.push_back(sub_str);
		}

		index = new_string.find(delim, 0);
		sub_str  = new_string.substr(0, index);
		new_string = new_string.substr(index+1);
	}

	if (sub_str != delim && sub_str.size() >= 1) {
		all_substr.push_back(sub_str);
	}

	return all_substr;
}

void request_handler(string &request, string &response) {
	char *read_file_string;
	long file_size = get_file_size("index.html");
	size_t get_request_index = request.find("GET");
	size_t post_request_index = request.find("POST");
	response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
	string content;

	if (get_request_index != string::npos) {
		vector<string> _req_buf_vec = split_string_by_delim(request, " ");
		string uri =  _req_buf_vec[1];

		if (uri == "/"){
			if (file_size)	{
				read_file_string = read_file("index.html", file_size+1);
				content = string(read_file_string);
			} else content = "There is no index.html file";
		} else {
			content = "Unhandle URL " + uri;
		}
	}

	if (post_request_index != string::npos) {
		content = "HTTP POST";
		vector<string> _content = split_string_by_delim(request, "\r\n\r\n");

		string input = _content[1];

		// Parse querystring
		regex pattern("([^&=]+)=([^&]*)");
		sregex_iterator begin = sregex_iterator(input.begin(), input.end(), pattern);
		sregex_iterator end	= sregex_iterator();

		for (sregex_iterator it = begin; it != end; it++) {
			smatch match = *it;
			string first = match[1].str();
			string second = match[2].str();
			cout << first << " " << second << endl;
		}
	}
	response += to_string(content.length()) + "\r\n\r\n" + content;
	return;
}
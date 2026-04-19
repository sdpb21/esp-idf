#include <iostream>
#include <vector>
#include <memory>
#include <algorithm> // find()
#include <functional>
#include <fstream>
#include "esp32_spiffs.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#define PORT			8000
#define MAXPENDING		5
#define MAX_CLIENTS		2	// Maximum numbers of connected HTTP clients to handle/monitor
#define WRITEFDS		NULL
#define EXCEPTFDS		NULL

#define BUFFSIZE		256

#define TIMEOUT			5 // seconds

#define SSID		"AP_SSID"
#define PASSWORD	"AP_PASSWORD"

extern "C" {
	void app_main();
}

using namespace std;

class HTTP_Server {
public:
	HTTP_Server(int port, function<void (string&, string&)> request_handler, string &request, string &response, bool reuse_address = true, int max_pending = MAXPENDING);
	void start_server();
private:
	int					_http_server_fd;
	int					_http_client_fd; // fd of the connected HTTP client
	vector<int>			_http_client_fd_list;
	int					_epfd;
	int					_port;
	int					_max_pending;
	char				ip_str[30];
	bool				_reuse_address;
	int					max_fd, sret; // select()
	struct				sockaddr_in http_client_addr;
	fd_set				readfds;
	struct				timeval timeout;
	socklen_t			_http_client_length;
	string				_httpd_hdr_str;
	string				_request, _response;

	int					socket_parameters_init();
	void				http_client_handler();
	function<void (string&, string&)> _request_handler;
};

void request_handler(string &request, string &response);
vector<string> split_string_by_delim(string s, string delim);

string request, response;
HTTP_Server http_server(PORT, request_handler, request, response);
SPIFFS spiffs;

void app_main() {
	WiFiClass WiFi(WIFI_MODE_AP);
	WiFi.ap_begin(SSID, PASSWORD);

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", SSID, PASSWORD);

	http_server.start_server();
}

HTTP_Server::HTTP_Server(int port, function<void (string&, string&)> request_handler, string &request, string &response, bool reuse_address, int max_pending) {
	_port = port;
	_reuse_address = reuse_address;
	_request = request;
	_response = response;

	_max_pending = max_pending;
	_request_handler = request_handler;

	_http_client_length = sizeof(http_client_addr);
	_http_client_fd_list = vector<int>(MAX_CLIENTS, 0);
	_httpd_hdr_str = "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n";
}

void HTTP_Server::start_server() {
	_http_server_fd = socket_parameters_init();

	if (_http_server_fd == -1) {
		cout << "Fail to start HTTP server. Please reset\n";
		while (1) {
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
	}

	max_fd = _http_server_fd;
	cout << "Waiting for a HTTP client to connect ...\n";
	while (1) {
		http_client_handler();
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void HTTP_Server::http_client_handler() {
	timeout.tv_sec = TIMEOUT; // Must set time out every time in the while loop
	FD_ZERO(&readfds);
	FD_SET(_http_server_fd, &readfds); // Must be inside while() loop to handle in every loop

	// Add client sockets to the set
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (_http_client_fd_list[i] > 0) {
			FD_SET(_http_client_fd_list[i], &readfds);
		}

		if (_http_client_fd_list[i] > max_fd) {
			max_fd = _http_client_fd_list[i];
		}
	}

	sret = select(max_fd + 1, &readfds, WRITEFDS, EXCEPTFDS, &timeout);
	if (sret < 0) {
		perror("select() error");
		return;
	}

	if (sret == 0) {
		cout << "Timeout after " << TIMEOUT << " seconds\n";
	} else {
		if (FD_ISSET(_http_server_fd, &readfds)) {
			if ((_http_client_fd = accept(_http_server_fd, (struct sockaddr *) &http_client_addr, &_http_client_length)) > 0) {
				char ip_str[30];
				inet_ntop(AF_INET, &(http_client_addr.sin_addr.s_addr), ip_str, INET_ADDRSTRLEN);
				cout << "New TCP sender with fd " << _http_client_fd << " connected with IP " << ip_str << endl;

				int _http_client_id = 0;

				// Add new HTTP client fd to _http_client_fd_list
				for (_http_client_id = 0; _http_client_id < MAX_CLIENTS; _http_client_id++) {
					if (_http_client_fd_list[_http_client_id] == 0) {
						_http_client_fd_list[_http_client_id] = _http_client_fd;
						cout << "Added new client to slot " << _http_client_id << endl;
						break;
					}
				}
				if (_http_client_id == MAX_CLIENTS) cout << MAX_CLIENTS << " HTTP clients have connected. Monitor no more connected HTTP clients\n";
			}
		}

		max_fd = _http_server_fd; // Reset max_fd to _http_server_fd in each loop
		for (int i = 0; i < MAX_CLIENTS; i++) {
			// Case: http_client_fd is bigger than the current max_fd
			if (_http_client_fd_list[i] > max_fd) {
				max_fd = _http_client_fd_list[i];
			}

			if (_http_client_fd_list[i]) {
				_http_client_fd = _http_client_fd_list[i];
				if (FD_ISSET(_http_client_fd, &readfds)) {
					char req_buf[BUFFSIZE]; // Buffer for HTTP request from HTTP client
					_request.clear();

					// Keep reading from _http_client_fd until end of headers
					while (true) {
						memset(req_buf, 0, BUFFSIZE);
						int n = read(_http_client_fd, req_buf, BUFFSIZE);;
						if (n <= 0) break;

						_request.append(req_buf, n);

						if (_request.find("\r\n\r\n") != std::string::npos) break;
					}

					if (_request.size() > 0) {
						_request_handler(_request, _response);
						if (!_response.empty()) {
							write(_http_client_fd, _response.c_str(), _response.length());
						}
					} else {
						vector<int>::iterator iter;
						iter = find(_http_client_fd_list.begin(), _http_client_fd_list.end(), _http_client_fd);
						if (iter != _http_client_fd_list.end()) {
							*iter = 0;
							cout << "HTTP client with fd " << _http_client_fd << " is disconnected\n";
						}
						close(_http_client_fd);
					}
				}
			}
		}
	}
}

int HTTP_Server::socket_parameters_init() {
	struct sockaddr_in http_server_addr;

	_http_server_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_http_server_fd == -1) {
		cout << "Fail to create HTTP server socket" << endl;
		return -1;
	} else cout << "Create HTTP server socket successfully\n";

	http_server_addr.sin_family = AF_INET;
	http_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	http_server_addr.sin_port = htons(_port);

	// setsockopt() must be called before bind() so that SO_REUSEADDR can take effect
	if (_reuse_address) {
		int enable_val = 1;
		if (!setsockopt(_http_server_fd, SOL_SOCKET, SO_REUSEADDR, &enable_val, sizeof(enable_val))) {
			cout << "Set socket to reuse address successfully\n";
		} else cout << "Unable to set socket to reuse address\n";
	}

	// Bind to the local address
	if (bind(_http_server_fd, (struct sockaddr *) &http_server_addr, sizeof(http_server_addr)) == -1) {
		cout << "Fail to bind socket to local address" << endl;
		return -1;
	}
	else cout << "Start TCP socket receiver successfully through binding\n";

	// Set up connection mode for socket sender
	if (listen(_http_server_fd, _max_pending) == -1) {
		cout << "listen() fails" << endl;
		return -1;
	}
	return _http_server_fd;
}

void request_handler(string &request, string &response) {
	size_t get_request_index = request.find("GET");
	size_t post_request_index = request.find("POST");
	response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
	string content;

	vector<string> _req_buf_vec = split_string_by_delim(request, " ");
	string uri = _req_buf_vec[1];

	if (get_request_index != string::npos) {
		if (uri == "/") {
			string file_data = spiffs.read_file("index.html");
			if (file_data != "NULL") content = file_data;
			else content = "There is no index.html file";
		} else {
			content = "Unhandle URL " + uri;
		}
	}

	if (post_request_index != string::npos) {
		/* HTTP headers and body (content) is always defined by a delimiter \r\n\r\n */
		size_t header_end = request.find("\r\n\r\n");

		string headers = request.substr(0, header_end);
		string body	= request.substr(header_end + 4);
		cout << "GPIO status is: " << body << endl;
	}

	response += to_string(content.length()) + "\r\n\r\n" + content;

	return;
}

vector<string> split_string_by_delim(string s, string delim) {
	vector<string> all_substr;
	size_t index = s.find(delim, 0);
	string sub_str = s.substr(0, index);
	string new_string = s.substr(index + delim.length());

	while (index != string::npos) {
		if (sub_str != delim && sub_str.size() >= 1) {
			all_substr.push_back(sub_str);
		}

		index = new_string.find(delim, 0);
		sub_str = new_string.substr(0, index);
		new_string = new_string.substr(index + 1);
	}

	if (sub_str != delim && sub_str.size() >= 1) {
		all_substr.push_back(sub_str);
	}

	return all_substr;
}
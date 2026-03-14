#include "http_server.h"

HTTP_Server::HTTP_Server(int port, function<void (string&, string&)> request_handler, string &request, string &response, bool reuse_address, int max_pending) {
	_port = port;
	_reuse_address = reuse_address;
	_request = request;
	_response = response;

	_max_pending = max_pending;
	_request_handler = request_handler;

	_http_client_length = sizeof(http_client_addr); // Get address size of sender
	_stop_server = false;
}

void HTTP_Server::start_server() {
	_stop_server = false; // Disable flag _stop_server when calling start_server() for the 2nd time
	_http_server_fd = socket_parameters_init();

	max_fd = _http_server_fd;
	cout << "Waiting for a HTTP client to connect ...\n";
	while (1) {
		http_client_handler();
		vTaskDelay(10 / portTICK_PERIOD_MS);
		if (_stop_server) break;
	}
}

void HTTP_Server::stop_server() {
	close(_http_server_fd); // Simply close the HTTP server fd to stop the HTTP server
	_stop_server = true;
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

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (_http_client_fd_list[i]) {
				_http_client_fd = _http_client_fd_list[i];
				if (FD_ISSET(_http_client_fd, &readfds)) {
					char req_buf[BUFFSIZE]; // Buffer for HTTP request from HTTP client
					bzero(req_buf, BUFFSIZE);

					int bytes_received = read(_http_client_fd, req_buf, BUFFSIZE);
					if (bytes_received > 0) {
						_request = req_buf;
						_request_handler(_request, _response);
						if (!_response.empty()) {
							write(_http_client_fd, _response.c_str(), _response.length());
						}
					} else {
						vector<int>::iterator iter;
						iter = find(_http_client_fd_list.begin(), _http_client_fd_list.end(), _http_client_fd);
						if(iter != _http_client_fd_list.end()) {
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
	struct 	sockaddr_in http_server_addr;

	// Create TCP socket receiver
	_http_server_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_http_server_fd == -1) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}
	cout << "Create HTTP server socket successfully\n";

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
		cout << "Fail to bind socket to local address with errno \n" << errno << endl;
		exit(EXIT_FAILURE);
	}
	cout << "Start TCP socket receiver successfully through binding\n";

	// Set up connection mode for socket sender
	if (listen(_http_server_fd, _max_pending) == -1) exit(EXIT_FAILURE);
	return _http_server_fd;
}

vector<string> HTTP_Server::split_string_by_delim(string s, string delim) {
	vector<string> all_substr;
	std::size_t index = s.find(delim, 0);
	string sub_str  = s.substr(0, index);
	string new_string = s.substr(index+delim.length());

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
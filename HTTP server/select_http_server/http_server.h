#include <iostream>
#include <sys/socket.h>	/* socket(), connect()*/
#include <stdlib.h>		/* atoi() and exit() */
#include <string.h>		/* memset() */
#include <unistd.h>		/* close() */
#include <fcntl.h>		/* open() */

#include <vector>
#include <algorithm> // find()
#include <memory>
#include <fstream>
#include <functional>

#include "sha.h"
#include "lwip/sockets.h"

#define MAXPENDING		5
#define MAX_CLIENTS		4	// Maximum numbers of connected HTTP clients to handle/monitor
#define WRITEFDS		NULL
#define EXCEPTFDS		NULL

#define BUFFSIZE		1000

#define TIMEOUT			5 // seconds

using namespace std;

class HTTP_Server {
public:
	HTTP_Server(int port, function<void (string&, string&)> request_handler, string &request, string &response, bool reuse_address = true, int max_pending = MAXPENDING);
	void start_server();
	void stop_server();
	bool switch_to_websocket(string request, string &response); // Switch protocol to WebSocket
	string ws_rx_data(string data, string &response);
	void ws_tx_data(string data);
	vector<string> split_string_by_delim(string s, string delim);
	bool _stop_server;
private:
	int 				_http_server_fd;
	int 				_http_client_fd; // fd of the connected HTTP client
	vector<int>			_http_client_fd_list = vector<int>(MAX_CLIENTS);
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
	string				_request, _response;

	int					socket_parameters_init();
	void				http_client_handler();
	function<void (string&, string&)> _request_handler;
	vector<uint8_t>		ws_frame; // WebSocket frame
};

enum class WS_FRAME : uint8_t {
	LAST_FRAME = 1, // FIN = 1, it’s the last frame for ws message
	MORE_FRAME = 0, // FIN = 0, more frames will follow in this ws message
	OPCODE_TXT_FRAME = 0x01, // Opcode: Text frame
	OPCODE_CONN_CLOSE = 0x08, // Opcode: Connection close
	FRAME_SND_FROM_CLIENT = 1,
	FRAME_SND_FROM_SERVER = 0,
};
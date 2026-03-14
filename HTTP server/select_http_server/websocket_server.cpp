#include "http_server.h"

// Switch protocol to WebSocket
bool HTTP_Server::switch_to_websocket(string request, string &response) {
	// Verify if the frame is a valid websocket frame
	size_t ws_key = request.find("Sec-WebSocket-Key:", 0);
	if ((request.find("Connection: Upgrade", 0) != string::npos || 
		request.find("Connection: keep-alive, Upgrade", 0) != string::npos)
		&& (request.find("Sec-WebSocket-Version: 13", 0) != string::npos)
		&& ((request.find("Chrome", 0) != string::npos && (request.find("Upgrade: websocket", 0) != string::npos)) 
		|| request.find("Firefox", 0) != string::npos)
		&& (ws_key != string::npos)) {

		// Size of Sec-WebSocket-Key: <24 bytes of ws key> is 43
		string sub_str  = request.substr(ws_key, 43);
		string ws_key = split_string_by_delim(sub_str, " ")[1];
		string ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		ws_key += ws_magic_string;

		SHA1 sha1;
		unique_ptr<uint32_t[]> sha_1_hash = sha1.hex_digest(ws_key);

		string _ws_key_str;

		// Convert SHA-1 to string
		for (int i = 0; i < 5; i++) {
			_ws_key_str += (char) (sha_1_hash[i] >> 24) & 0xFF;
			_ws_key_str += (char) (sha_1_hash[i] >> 16) & 0xFF;
			_ws_key_str += (char) (sha_1_hash[i] >> 8) & 0xFF;
			_ws_key_str += (char) sha_1_hash[i] & 0xFF;
		}

		// WebSocket handshake process requires a Base64-encoded SHA-1
		_ws_key_str = base64_encoding(_ws_key_str);
		
		response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n";
		response += "Connection: Upgrade\r\nSec-WebSocket-Accept: ";
		response += _ws_key_str + "\r\n\r\n";

		cout << response << endl;
		return true;
	} else return false;
}

// Read ws message from ws client then form the frame to response to this ws client
string HTTP_Server::ws_rx_data(string data, string &response) {
	string client_payload;
	uint8_t fin = data[0] >> 7 & 0x01;
	if (fin == static_cast<uint8_t>(WS_FRAME::LAST_FRAME)) {
		// cout << "This is the last frame in this ws message\n";
	}
	
	uint8_t rsv1 = (data[0] >> 6) & 0b01;
	uint8_t rsv2 = (data[0] >> 5) & 0b001;
	uint8_t rsv3 = (data[0] >> 4) & 0b0001;
	uint8_t opcode = data[0] & 0b1111;

	// Parse for the payload sent from ws client
	if (opcode == static_cast<uint8_t>(WS_FRAME::OPCODE_TXT_FRAME)) {
		// cout << "Receive text frame from ws client\n";
		uint8_t mask = data[1] >> 7 & 0x01;
		uint8_t payload_length = data[1] & 0b01111111;
		if (mask == static_cast<uint8_t>(WS_FRAME::FRAME_SND_FROM_CLIENT)
			&& payload_length != 126 && payload_length != 127) {
			vector<uint8_t> masking_key, payload;
			masking_key.push_back(data[2]);
			masking_key.push_back(data[3]);
			masking_key.push_back(data[4]);
			masking_key.push_back(data[5]);

			uint8_t _index = 6;
			
			for (uint8_t i = 0; i < payload_length; i++) {
				payload.push_back(data[_index]);
				payload[i] ^= masking_key[i % 4];
				_index += 1;
				client_payload += string(1, payload[i]);
			}      

			// Form the response for ws client: After receiving the ws frame from the client,
			// server must reponses a valid websocket frame to the client.
			ws_frame.push_back(data[0]);// Keep the first byte as the frame sent from ws client
			uint8_t ws_server_payload = 0;

			ws_server_payload &= 0x7f; // 0b01111111 for MASK = 0 (FRAME_SND_FROM_SERVER)
			// Let payload len = 0

			ws_frame.push_back(ws_server_payload);

			response = string(ws_frame.begin(), ws_frame.end());

			// cout << client_payload << endl;
		}
		return client_payload;
	} else {
		cout << "Connection close\n";
		return "Connection close";
	}
}

void HTTP_Server::ws_tx_data(string data) {
	ws_frame.clear();
	uint8_t first_byte = 0; 
	first_byte = static_cast<uint8_t>(WS_FRAME::LAST_FRAME) << 7;
	// rsv1, rsv2, rsv3 = 0
	first_byte |= static_cast<uint8_t>(WS_FRAME::OPCODE_TXT_FRAME); // opcode

	ws_frame.push_back(first_byte);

	//ws server payload as the second byte
	uint8_t ws_server_payload = 0; // Let MASK = 0 (FRAME_SND_FROM_SERVER)
	ws_frame.push_back(data.length()); // Payload length
	if (data.length() != 126 && data.length() != 127) {
		for (int i = 0; i < data.length(); i++) ws_frame.push_back(static_cast<uint8_t>(data[i]));
	}

	string ws_server_tx = string(ws_frame.begin(), ws_frame.end());
	write(_http_client_fd, ws_server_tx.c_str(), ws_server_tx.length());
}

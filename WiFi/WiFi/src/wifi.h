#ifndef _INC_WIFI
#define _INC_WIFI

#include <iostream>
#include <string>
#include <cstring> 
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"

#define ESP32_WIFI_CHANNEL		8
#define MAX_STA_CONNECTED		4
#define WIFI_STA_STARTED		0 // WiFi station started
#define WIFI_STA_CONNECTED		1 // WiFi station connected
#define WIFI_STA_DISCONNECTED	2 // WiFi station disconnected

#define WIFI_AP_CONNECTED		0
#define WIFI_AP_STOP			1
#define WIFI_AP_START			2

#define DEFAULT_SCAN_LIST_SIZE	10

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
	static const char *TAG = "wifi TAG:";

	class WiFiClass {
	public:
		WiFiClass(wifi_mode_t wifi_mode, wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA2_PSK);
		void begin(string ssid, string password);
		void ap_begin(string ssid, string password);
		void ap_stop();
		void disconnect();
		uint32_t sta_status();
		uint32_t ap_status();
		void start_scanning();
		void display_all_ap_info();
		string get_ssid_rssi();
	private:
		static wifi_config_t sta_config, ap_config; // Must be static for the esp-idf WiFi driver
		static string _ssid, _password;
		static string _ap_ssid, _ap_password;
		static uint32_t _sta_state, _ap_state;
		uint16_t total_ap = DEFAULT_SCAN_LIST_SIZE;
		wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
		uint16_t ap_count = 0;
		static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
		void print_auth_mode(int authmode);
		void print_cipher_type(int pairwise_cipher, int group_cipher);
	};
#ifdef __cplusplus
}
#endif

#endif
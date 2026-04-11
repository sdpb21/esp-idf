#include "wifi.h"

/*
	Initialize dummy value for those static variables
*/
string WiFiClass::_ap_ssid		= "", WiFiClass::_ssid = "";
string WiFiClass::_ap_password = "", WiFiClass::_password = "";
wifi_config_t WiFiClass::sta_config, WiFiClass::ap_config; 
uint32_t WiFiClass::_sta_state = WIFI_STA_DISCONNECTED;
uint32_t WiFiClass::_ap_state = WIFI_AP_STOP;

WiFiClass::WiFiClass(wifi_mode_t wifi_mode, wifi_auth_mode_t auth_mode) {
	// Initialize NVS
	static bool is_init = false; // prevent second call for this init function
	if(is_init == true) return;
	is_init = true;

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	esp_netif_init();
	esp_event_loop_create_default();

	esp_netif_create_default_wifi_ap();
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);

	// Station mode setup
	if ((wifi_mode == WIFI_MODE_STA) || (wifi_mode == WIFI_MODE_APSTA)) {
		sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
		sta_config.sta.pmf_cfg.capable = true;
		sta_config.sta.pmf_cfg.required = false;
	}

	// Access point mode setup
	if ((wifi_mode == WIFI_MODE_AP) || (wifi_mode == WIFI_MODE_APSTA)) {
		ap_config.ap.channel = ESP32_WIFI_CHANNEL;
		ap_config.ap.max_connection = MAX_STA_CONNECTED;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode));
}

void WiFiClass::wifi_event_handler(void* arg, esp_event_base_t event_base,
									int32_t event_id, void* event_data) {
	if (event_base == WIFI_EVENT){
		switch (event_id) {
			case WIFI_EVENT_STA_START:
			{
				esp_wifi_connect();
				_sta_state = WIFI_STA_STARTED;
				break;
			}
			case WIFI_EVENT_STA_DISCONNECTED: 
			{
				ESP_LOGI(TAG,"connect to the AP fail");
				_sta_state = WIFI_STA_DISCONNECTED;
				esp_wifi_connect();
				break;
			}
			case WIFI_EVENT_STA_CONNECTED:
			{
				printf("WiFi is connected to SSID: %s with password %s\n", _ssid.c_str(), _password.c_str());
				_sta_state = WIFI_STA_CONNECTED;
				break;
			}
			case WIFI_EVENT_AP_STACONNECTED:
			{
				wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
				ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
				_ap_state = WIFI_AP_CONNECTED;
				break;
			}
			case WIFI_EVENT_AP_STADISCONNECTED:
			{
				wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
				ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
				break;
			}
			case WIFI_EVENT_AP_START:
			{
				ESP_LOGI(TAG, "AP %s starts", _ap_ssid.c_str());
				_ap_state = WIFI_AP_START;
				break;
			}
			case WIFI_EVENT_AP_STOP:
			{
				ESP_LOGI(TAG, "AP %s stops", _ap_ssid.c_str());
				_ap_state = WIFI_AP_STOP;
				break;
			}
		}
	} else if (event_base == IP_EVENT) {
		if (event_id == IP_EVENT_STA_GOT_IP) {
			ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
			ESP_LOGI(TAG, "Station IP:" IPSTR, IP2STR(&event->ip_info.ip));
		}
	}
}

void WiFiClass::begin(string ssid, string password) {
	strcpy((char*)sta_config.sta.ssid, ssid.c_str());
	strcpy((char*)sta_config.sta.password, password.c_str());

	_ssid = ssid;
	_password = password;

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiClass::ap_begin(string ssid, string password) {
	strcpy((char*)ap_config.ap.ssid, ssid.c_str());
	strcpy((char*)ap_config.ap.password, password.c_str());

	_ap_ssid = ssid;
	_ap_password = password;

	ap_config.ap.ssid_len = ssid.length();

	if (ssid.length() == 0) {
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
	} else ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiClass::ap_stop() {
	strcpy((char*)ap_config.ap.ssid, "");
	strcpy((char*)ap_config.ap.password, "");
	ap_config.ap.ssid_hidden = true;
	ap_config.ap.authmode = WIFI_AUTH_OPEN;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_stop());
}

uint32_t WiFiClass::sta_status() {
	return _sta_state;
}

uint32_t WiFiClass::ap_status() {
	return _ap_state;
}

void WiFiClass::disconnect() {
	// we have to clear the internal config for both STA and AP mode
	// to allow an clean wifi section at the next wifi start
	// Furthermore, ssid and password from STA mode is designed and managed
	// in our spiffs config, instead of using ESP32 nvram
	// strcpy((char*)ap_config.ap.ssid, "");
	// strcpy((char*)ap_config.ap.password, "");
	// ap_config.ap.authmode = WIFI_AUTH_OPEN;
	// ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

	strcpy((char*)sta_config.sta.ssid, "");
	strcpy((char*)sta_config.sta.password, "");
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

	esp_wifi_stop(); // Disconnect the currently connected WiFi to connect to the latest setup WiFi
}
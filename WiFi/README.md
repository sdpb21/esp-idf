# [WiFi](WiFi) library

[WiFi](WiFi) is a component structured from the ``esp_wifi`` built-in component raw API.

[WiFi](WiFi) 1.0.0 version: Release in 13th Jan 2025

ESP32 WiFi runs on the [ESP32 WiFi stack](https://github.com/espressif/esp32-wifi-lib/tree/master), which has only the static library file (``.a``) not the source code. 

# Implementations from WiFi library

* [station.cpp](src/station.cpp): Working as a station
## Note

1. Musn't put ``WiFiClass`` class object as a global variable, i.e before ``app_main()``, if it isn't a pointer

In [station.cpp](src/station.cpp), if putting ``WiFiClass`` class object as a global variable, i.e running before ``app_main()`` like this, board will reset:

```cpp
// MUST NOT DO THIS
WiFiClass WiFi(WIFI_MODE_STA);

void app_main() {
	WiFi.begin(SSID, PASSWORD);
}
```

2. **Limit the max time for ESP32 to retry connect**

For ``STA`` and ``AP+STA`` mode, to limit the maximum time for ESP32 to retry connect, use this method:

```c
#define AP_MAXIMUM_RETRY_CONNECT 3 // Number of times retry to connect to the AP

void wifi_event_handler(void* arg, esp_event_base_t event_base,
						int32_t event_id, void* event_data) {
// Operations go here
	case WIFI_EVENT_STA_DISCONNECTED:
	if (ap_retry_connect < AP_MAXIMUM_RETRY_CONNECT) {
		esp_wifi_connect();
		ap_retry_connect++;
		ESP_LOGI(TAG, "retry to connect to the AP");
	}
	ESP_LOGI(TAG,"connect to the AP fail");
// ...
}
```
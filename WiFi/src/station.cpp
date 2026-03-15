#include "wifi.h"

#define SSID		"SSID"
#define PASSWORD	"jnrn5941"

extern "C" {
	void app_main();
}

void app_main() {
	WiFiClass WiFi(WIFI_MODE_STA);
	WiFi.begin(SSID, PASSWORD);
	while (WiFi.sta_status() != WIFI_STA_CONNECTED) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
		cout << ".";
	}
	cout << "\nWiFi connected\n";
}

/*
If putting WiFiClass class object as a global variable, i.e running before app_main() like this, board will reset:

// MUST NOT DO THIS
WiFiClass WiFi(WIFI_MODE_STA);

void app_main() {
	WiFi.begin(SSID, PASSWORD);
}
*/
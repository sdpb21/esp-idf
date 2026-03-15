CMakeLists file set up for every program that call ``WiFi`` library: [CMakeLists.txt](CMakeLists.txt)

# API

```c
WiFiClass(wifi_mode_t wifi_mode, wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA2_PSK);
```
Constructor ``WiFiClass``:
* Initialize default parameter
* Set WiFi working mode. Acceptable mode are ``STA``, ``AP`` and ``STA+AP``.

```c
void begin(string ssid, string password);
```

Connect to an existed access point with ssid and password

```c
void ap_begin(string ssid, string password);
```
Create an AP with ``ssid`` and ``password``

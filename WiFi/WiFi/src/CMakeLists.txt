set(COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_SRCS "wifi.cpp" "wifi_scan.cpp")
set(COMPONENT_REQUIRES nvs_flash esp_wifi)

register_component()

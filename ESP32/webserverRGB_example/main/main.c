#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include <esp_https_server.h>
#include <string.h>
#include "driver/gpio.h"
#include <stdio.h>

#define ledR 33
#define ledG 25
#define ledB 26

int8_t led_r_state = 0;
int8_t led_g_state = 0;
int8_t led_b_state = 0;

#if CONFIG_EXAMPLE_CONNECT_WIFI

#define EXAMPLE_NETIF_DESC_STA "example_netif_sta"
static esp_netif_t *s_example_sta_netif = NULL;
static const char *TAG = "main";
static int s_retry_num = 0;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;

#if CONFIG_EXAMPLE_WIFI_SCAN_METHOD_FAST
#define EXAMPLE_WIFI_SCAN_METHOD WIFI_FAST_SCAN
#elif CONFIG_EXAMPLE_WIFI_SCAN_METHOD_ALL_CHANNEL
#define EXAMPLE_WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_EXAMPLE_WIFI_CONNECT_AP_BY_SIGNAL
#define EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_EXAMPLE_WIFI_CONNECT_AP_BY_SECURITY
#define EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_EXAMPLE_WIFI_AUTH_OPEN
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_EXAMPLE_WIFI_AUTH_WEP
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_EXAMPLE_WIFI_AUTH_WPA_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_EXAMPLE_WIFI_AUTH_WPA2_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_EXAMPLE_WIFI_AUTH_WPA_WPA2_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_EXAMPLE_WIFI_AUTH_WPA2_ENTERPRISE
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_EXAMPLE_WIFI_AUTH_WPA3_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_EXAMPLE_WIFI_AUTH_WPA2_WPA3_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_EXAMPLE_WIFI_AUTH_WAPI_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#if CONFIG_EXAMPLE_CONNECT_IPV6
#define MAX_IP6_ADDRS_PER_NETIF (5)
static SemaphoreHandle_t s_semph_get_ip6_addrs = NULL;
/* types of ipv6 addresses to be displayed on ipv6 events */
const char *example_ipv6_addr_types_to_str[6] = {
    "ESP_IP6_ADDR_IS_UNKNOWN",
    "ESP_IP6_ADDR_IS_GLOBAL",
    "ESP_IP6_ADDR_IS_LINK_LOCAL",
    "ESP_IP6_ADDR_IS_SITE_LOCAL",
    "ESP_IP6_ADDR_IS_UNIQUE_LOCAL",
    "ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6"
};

#if defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_LOCAL_LINK)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_LINK_LOCAL
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_GLOBAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_GLOBAL
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_SITE_LOCAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_SITE_LOCAL
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_UNIQUE_LOCAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_UNIQUE_LOCAL
#endif // if-elif CONFIG_EXAMPLE_CONNECT_IPV6_PREF_...

#endif  // #if CONFIG_EXAMPLE_CONNECT_IPV6

esp_err_t init_led(void);
esp_err_t toggle_led(int led);
static esp_err_t rgb_example_wifi_sta_do_disconnect(void);

/* An HTTP GET handler */
// Handler function registered in URI structure, to be called with a supported request method
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGE(TAG, "inside root_get_handler");
    /* The next 2 lines are useful in case you wish to make a file with binary or text data to be
       available to your component and you don't want to reformat the file as a C source, in our
       case we want to embed the html code stored in the view.html file, we must first specify the
       argument EMBED_TXTFILES in the component registration (CMakeLists.txt file) as follows:
        
       idf_component_register(...
                   EMBED_TXTFILES "view.html")
        
       This will embed the contents of the text file wiew.html as a null-terminated string. The
       file contents will be added to the .rodata section in flash memory, and are available via
       symbol names as can be read in the next 2 lines: */
    extern unsigned char view_start[] asm("_binary_view_html_start");
    extern unsigned char view_end[] asm("_binary_view_html_end");
    // Get the lenght of the html file stored as a string
    size_t view_len = view_end - view_start;
    char viewHtml[view_len];                // Creates a string with the lenght of the html file
    /* Next line copy view_len bytes starting from address pointed by view_start to address 
       pointed by viewHtml */
    memcpy(viewHtml, view_start, view_len);
    ESP_LOGI(TAG, "URI: %s", req->uri);

    // This turns on or off the red LED according to his actual state
    if (strcmp(req->uri, "/?led-r") == 0)
    {
        toggle_led(ledR);
    }
    // This turns on or off the green LED according to his actual state
    if (strcmp(req->uri, "/?led-g") == 0)
    {
        toggle_led(ledG);
    }
    // This turns on or off the blue LED according to his actual state
    if (strcmp(req->uri, "/?led-b") == 0)
    {
        toggle_led(ledB);
    }

    char *viewHtmlUpdated;  // Pointer to a char
    /* Allocates a storage large enough to hold the output (viewHtml) including the terminated 
       null character, returns a pointer to that storage via the first argument (viewHtmlUpdated),
       the pointer should be passed to free to release the allocated storage when it's no longer
       needed. Returns the number of characters written */
    int formattedStrResult = asprintf(&viewHtmlUpdated, viewHtml, led_r_state ? "ON" : "OFF", led_g_state ? "ON" : "OFF", led_b_state ? "ON" : "OFF");

    /* Next line sets the 'Content type' field of the HTTP response to text/html, but it isn't 
       sent out until any of the send APIs is executed */
    httpd_resp_set_type(req, "text/html");

    // If the number of characters returned by asprintf is greater than 0, then...
    if (formattedStrResult > 0)
    {
        /* Send a complete HTML response, assumes that the entire response is in a buffer
           (viewHtmlUpdated) */
        httpd_resp_send(req, viewHtmlUpdated, view_len);
        free(viewHtmlUpdated);
    }
    else
    {
        ESP_LOGE(TAG, "Error updating variables");
        httpd_resp_send(req, viewHtml, view_len);
    }

    return ESP_OK;
}

// URI handler structure
static const httpd_uri_t root = {
    .uri = "/",                 // Uniform Resource Identifier
    .method = HTTP_GET,         // Method supported by the URI
    .handler = root_get_handler // Handler to call for supported request method
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    ESP_LOGE(TAG, "inside start_webserver");

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

    // Define and initialize an HTTPS server config struct, with default values
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    /* Transport mode insecure (SSL disabled) to start the server without SSL, this is used for
       testing or to use it in trusted environments where you prefer speed over security */
    conf.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
    // Creates a SSL (Secure Sockets Layer) capable HTTP server (SSL is diabled this time)
    esp_err_t ret = httpd_ssl_start(&server, &conf);
    // If there is an error starting the server, exit
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    // Registers an URI (Uniform Resource Identifier) handler
    httpd_register_uri_handler(server, &root);
    return server;
}

static void stop_webserver(httpd_handle_t server)
{
    ESP_LOGE(TAG, "inside stop_webserver");
    // Stop the httpd server
    httpd_ssl_stop(server);
}

// Handler function to stop the webserver when station is disconnected from WiFi
static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    ESP_LOGE(TAG, "inside disconnect_handler");
    if (*server)
    {
        stop_webserver(*server);
        *server = NULL;
    }
}

// Handler function that is called to start the webserver when the station got an IP
static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    ESP_LOGE(TAG, "inside connect_handler");
    if (*server == NULL)
    {
        *server = start_webserver();
    }
}


esp_err_t init_led(void)
{
    ESP_LOGE(TAG, "inside init_led");
    gpio_config_t pGPIOConfig;                          // Declares the struct to config the GPIO
    pGPIOConfig.pin_bit_mask = (1ULL << ledR)           // Set pins to be used
                             | (1ULL << ledG) 
                             | (1ULL << ledB);
    pGPIOConfig.mode = GPIO_MODE_DEF_OUTPUT;            // GPIO pins as outputs
    pGPIOConfig.pull_up_en = GPIO_PULLUP_DISABLE;       // GPIO pull-up resistors disabled
    pGPIOConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;   // GPIO pull-down resistors disabled
    pGPIOConfig.intr_type = GPIO_INTR_DISABLE;          // GPIO interruptions disabled


    gpio_config(&pGPIOConfig);                          // Configures the GPIO pins


    ESP_LOGI(TAG, "init led completed");
    return ESP_OK;
}

// Function to turn on and off LEDs
esp_err_t toggle_led(int led)
{
    ESP_LOGE(TAG, "inside toggle_led");
    int8_t state = 0;
    switch (led)
    {
    case ledR:
        led_r_state = !led_r_state;
        state = led_r_state;
        break;
    case ledG:
        led_g_state = !led_g_state;
        state = led_g_state;
        break;
    case ledB:
        led_b_state = !led_b_state;
        state = led_b_state;
        break;


    default:
        gpio_set_level(ledR, 0);
        gpio_set_level(ledG, 0);
        gpio_set_level(ledB, 0);
        led_r_state = 0;
        led_g_state = 0;
        led_b_state = 0;
        break;
    }
    gpio_set_level(led, state);
    return ESP_OK;
}

static void rgb_example_wifi_start(void)
{
    ESP_LOGE(TAG, "inside rgb_example_wifi_start");
    /* Initializes the configuration structure parameters with default values to be passed to
       esp_wifi_init call */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /** Initializes the WiFi, allocates resources for WiFi drivers such as WiFi control structure,
     *  RX/TX buffer, WiFi NVS structure, etc. Also starts WiFi task. This API must be called
     *  before all other WiFi APIs must be called. You must always use WIFI_INIT_CONFIG_DEFAULT to
     *  set the configuration default values, this guarantees that all the fields get the right
     *  value when more fields are added to wifi_init_config_t structure in a future release. If
     *  you want to set your own initial values, overwrite the default values which are setted by
     *  WIFI_INIT_CONFIG_DEFAULT. Please be notified that the field 'magic' of wifi_init_config_t
     *  should always be WIFI_INIT_CONFIG_MAGIC! (THIS IS WEIRD, DIG INTO THIS IN THE FUTURE),
     *  and check for errors */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Define an ESP-netif (NETwork InterFace) inherent config struct and initializes it with
       default parameters */
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    /* Warning: the interface desc is used in tests to capture actual connection details (IP, gw,
       mask) */
    // Change the interface description initialized with ESP_NETIF_INHERENT_DEFAULT_WIFI_STA
    esp_netif_config.if_desc = EXAMPLE_NETIF_DESC_STA;
    /* Change the priority of the interface from it's default value setted with ESP_NETIF_INHERENT
       _DEFAULT_WIFI_STA to 128, the higher the value, the higher the priority */
    esp_netif_config.route_prio = 128;
    /* Creates esp_netif WiFi object of STATION type, based on the custom configuration and 
       returns a pointer to the esp_netif instance */
    s_example_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    // Set default handlers for station
    esp_wifi_set_default_wifi_sta_handlers();

    // Set the WiFi API configuration storage type, the configuration will be stored in memory
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // Set the WiFi operating mode as station mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* Starts WiFi according to current configuration, creates a station control block and starts
       the station because the mode is station */
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void rgb_example_handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGE(TAG, "inside rgb_example_handler_on_wifi_disconnect");
    s_retry_num++;
    /* The next if block is executed when the actual number of connection retries is bigger than
       the default maximum number of retries, it gives the binary semaphore and unregisters the
       handler functions */
    if (s_retry_num > CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        /* let example_wifi_sta_do_connect() return */
        if (s_semph_get_ip_addrs) {
            xSemaphoreGive(s_semph_get_ip_addrs);
        }
#if CONFIG_EXAMPLE_CONNECT_IPV6
        if (s_semph_get_ip6_addrs) {
            xSemaphoreGive(s_semph_get_ip6_addrs);
        }
#endif
        /* Unregisters the handler functions previously registered, deletes the binary semaphore
           and disconnects the WiFi station from the access point */
        rgb_example_wifi_sta_do_disconnect();   // defined
        return;
    }
    wifi_event_sta_disconnected_t *disconn = event_data;
    /* The next if block checks for the disconnection reason, if the reason is roaming, then
       returns from the actual function to the point where it was called */
    if (disconn->reason == WIFI_REASON_ROAMING) {
        ESP_LOGD(TAG, "station roaming, do nothing");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected %d, trying to reconnect...", disconn->reason);
    /*  Connects the WiFi station to the access point, this only works in softAP+station and
        station modes */
    esp_err_t err = esp_wifi_connect();
    /* The next if block checks for the value returned by esp_wifi_connect() call, if the WiFi was
       not started by a call to the function esp_wifi_start, then the actual handler function will
       return */
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    /* Check for the error code returned by a call to esp_wifi_connect() function, terminates the
       program if code is no ESP_OK */
    ESP_ERROR_CHECK(err);
}

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
bool rgb_example_is_our_netif(const char *prefix, esp_netif_t *netif)
{
    ESP_LOGE(TAG, "inside rgb_example_is_our_netif");
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static void rgb_example_handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ESP_LOGE(TAG, "inside rgb_example_handler_on_sta_got_ip");
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    /* Next if block returns if the string stored in EXAMPLE_NETIF_DESC_STA is the same as the
       stored in the network interface description from the event structure */
    if (!rgb_example_is_our_netif(EXAMPLE_NETIF_DESC_STA, event->esp_netif)) {  // defined
        return;
    }
    /* Prints the network interface description given by the esp_netif_get_desc function call and
       the IP address given by the IP2STR macro, the IP is stored in a utint32_t type number in
       different bytes and the macro returns those bytes separated */
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) { // if semaphore is created or taken (not NULL), give it
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}

static void rgb_example_handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    ESP_LOGE(TAG, "inside rgb_example_handler_on_wifi_connect");
#if CONFIG_EXAMPLE_CONNECT_IPV6
    /* Creates interface link-local IPv6 address. Causes the TCP/IP stack to create a link-local
       IPv6 address for the specified interface */
    esp_netif_create_ip6_linklocal(esp_netif);
#endif // CONFIG_EXAMPLE_CONNECT_IPV6
}

static void rgb_example_handler_on_sta_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGE(TAG, "inside rgb_example_handler_on_sta_got_ipv6");
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    /* Next if block returns if the string stored in EXAMPLE_NETIF_DESC_STA is the same as the
       stored in the network interface description from the event structure */
    if (!rgb_example_is_our_netif(EXAMPLE_NETIF_DESC_STA, event->esp_netif)) {  // defined
        return;
    }
    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    /* Prints the network interface description given by the esp_netif_get_desc function call and
       the IPv6 address given by the IPV62STR macro */
    ESP_LOGI(TAG, "Got IPv6 event: Interface \"%s\" address: " IPV6STR ", type: %s", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip), example_ipv6_addr_types_to_str[ipv6_type]);

    if (ipv6_type == EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE) {
        if (s_semph_get_ip6_addrs) {
            xSemaphoreGive(s_semph_get_ip6_addrs);
        } else {
            ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %s", IPV62STR(event->ip6_info.ip), example_ipv6_addr_types_to_str[ipv6_type]);
        }
    }
}

static esp_err_t rgb_example_wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    ESP_LOGE(TAG, "inside rgb_example_wifi_sta_do_connect");
    if (wait) {
        // This time the binary semaphore is for take the IP as shared resourse
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();    // Creates a binary semaphore handle
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
#if CONFIG_EXAMPLE_CONNECT_IPV6
        // Creates a binary semaphore handle for IPv6
        s_semph_get_ip6_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip6_addrs == NULL) {
            vSemaphoreDelete(s_semph_get_ip_addrs);
            return ESP_ERR_NO_MEM;
        }
#endif
    }
    // Clears the number of connection retries
    s_retry_num = 0;
    /* Registers a WiFi event to the system event loop, when the station is disconnected, the 
       function rgb_example_handler_on_wifi_disconnect (that is the handler registered) gets
       called, later, check for errors */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &rgb_example_handler_on_wifi_disconnect, NULL));
    /* Registers an IP event to the system event loop, when the station got an IP, the handler
       function rgb_example_handler_on_sta_got_ip gets called, after that, check for errors */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &rgb_example_handler_on_sta_got_ip, NULL));
    /* Registers a WiFi event to the system event loop, when the station is connected, the handler
       function rgb_example_handler_on_wifi_connect gets called, and then, check for errors */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &rgb_example_handler_on_wifi_connect, s_example_sta_netif));
#if CONFIG_EXAMPLE_CONNECT_IPV6
    /* Registers an IP event to the system event loop, when the station got an IPv6, the handler
       function rgb_example_handler_on_sta_got_ipv6 gets called, after that, check for errors */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &rgb_example_handler_on_sta_got_ipv6, NULL));
#endif

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    /** Set the configuration of the WiFi module as station interface with the values given in the
     *  structure wifi_config and check for errors, if returned value is not ESP_OK, terminates
     *  the program.
     * */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    /*  Connects the WiFi station to the access point, this only works in softAP+station and
        station modes */
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }
    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP(s)");
#if CONFIG_EXAMPLE_CONNECT_IPV4
        /* Takes the semaphore token for the IPv4 if it's given (the shared resourse is the IP),
           if not, waits a quantity of time given in ticks by portMAX_DELAY, for the token to be
           given */
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
#endif
#if CONFIG_EXAMPLE_CONNECT_IPV6
        /* Takes the semaphore token for the IPv6 if it's given, if not, waits a quantity of time
           given in ticks by portMAX_DELAY, for the token to be given */
        xSemaphoreTake(s_semph_get_ip6_addrs, portMAX_DELAY);
#endif
        // If number of connection retries is above the maximum configured, return ESP_FAIL
        if (s_retry_num > CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t rgb_example_wifi_connect(void)
{
    ESP_LOGE(TAG, "inside rgb_example_wifi_connect");
    ESP_LOGI(TAG, "Start example_connect.");
    // Executes the first steps to start a WiFi connection according to documentation
    rgb_example_wifi_start();           // defined
    // Configures the station device
    wifi_config_t wifi_config = {
        .sta = {
#if !CONFIG_EXAMPLE_WIFI_SSID_PWD_FROM_STDIN
            .ssid = CONFIG_EXAMPLE_WIFI_SSID,
            .password = CONFIG_EXAMPLE_WIFI_PASSWORD,
#endif
            .scan_method = EXAMPLE_WIFI_SCAN_METHOD,
            .sort_method = EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = CONFIG_EXAMPLE_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };

    /* Configures the ESP32 as station and try to connect it to the access point previously
       configured, returns ESP_OK if connection was successful and the station got the IP */
    return rgb_example_wifi_sta_do_connect(wifi_config, true);  // defined

}

static void rgb_example_wifi_stop(void)
{
    ESP_LOGE(TAG, "inside rgb_example_wifi_stop");
    /* Stops the WiFi station and frees the station control block */
    esp_err_t err = esp_wifi_stop();
    // if esp_wifi_init function wasn't executed yet, then returns
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    // Check the value returned by esp_wifi_stop for errors, if not ESP_OK, stops the program
    ESP_ERROR_CHECK(err);
    // Free all resources allocated in esp_wifi_init and stop WiFi task and check for errors
    ESP_ERROR_CHECK(esp_wifi_deinit());
    /* Clears default WiFi event handlers for supplied network interface (NETIF) and check for
       errors, stops the program if not ESP_OK */
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_sta_netif));
    // Destroys the esp_netif object
    esp_netif_destroy(s_example_sta_netif);
    // Sets the esp_netif object to NULL
    s_example_sta_netif = NULL;
}

static void rgb_example_wifi_shutdown(void)
{
    ESP_LOGE(TAG, "inside rgb_example_wifi_shutdown");
    /* Unregisters the handler functions previously registered, deletes the binary semaphore
       and disconnects the WiFi station from the access point */
    rgb_example_wifi_sta_do_disconnect();   // defined
    /* Stops the WiFi station and frees the station control block, frees all resources allocated
       in esp_wifi_init and stops WiFi task, clears default WiFi event handlers for supplied
       network interface (NETIF) and destroys the esp_netif object */
    rgb_example_wifi_stop();                // defined
}

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool example_is_our_netif(const char *prefix, esp_netif_t *netif)
{
    ESP_LOGE(TAG, "inside example_is_our_netif");
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static esp_err_t print_all_ips_tcpip(void* ctx)
{
    ESP_LOGE(TAG, "inside print_all_ips_tcpip");
    const char *prefix = ctx;
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t *netif = NULL;
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        if (example_is_our_netif(prefix, netif)) {
            ESP_LOGI(TAG, "Connected to %s", esp_netif_get_desc(netif));
#if CONFIG_EXAMPLE_CONNECT_IPV4
            esp_netif_ip_info_t ip;
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&ip.ip));
#endif
#if CONFIG_EXAMPLE_CONNECT_IPV6
            esp_ip6_addr_t ip6[MAX_IP6_ADDRS_PER_NETIF];
            int ip6_addrs = esp_netif_get_all_ip6(netif, ip6);
            for (int j = 0; j < ip6_addrs; ++j) {
                esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&(ip6[j]));
                ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %s", IPV62STR(ip6[j]), example_ipv6_addr_types_to_str[ipv6_type]);
            }
#endif
        }
    }
    return ESP_OK;
}

static void rgb_example_print_all_netif_ips(const char *prefix)
{
    ESP_LOGE(TAG, "inside rgb_example_print_all_netif_ips");
    /* Print all IPs in TCPIP context to avoid potential races of removing/adding netifs when
       iterating over the list */
    esp_netif_tcpip_exec(print_all_ips_tcpip, (void*) prefix);
}

static esp_err_t rgb_example_connect(void)
{
    ESP_LOGE(TAG, "inside rgb_example_connect");
#if CONFIG_EXAMPLE_CONNECT_WIFI
    /* rgb_example_wifi_connect starts the WiFi connection, configures the ESP32 as station and
       try to connect it to the access point, returns ESP_OK if everything is ok */
    if (rgb_example_wifi_connect() != ESP_OK) { // defined
        return ESP_FAIL;
    }
    /* Registers a handler function (rgb_example_wifi_shutdown) that gets invoked before the
       application is restarted using esp_restart function and check for errors */
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&rgb_example_wifi_shutdown)); // handler defined
    /* Calls to a function that calls to a pointer to a callback function that has as argument the
       string defined by the EXAMPLE_NETIF_DESC_STA constant and prints all IPs in the TCP/IP
       context */
    rgb_example_print_all_netif_ips(EXAMPLE_NETIF_DESC_STA);                    // defined
#endif

    return ESP_OK;
}

static esp_err_t rgb_example_wifi_sta_do_disconnect(void)
{
    ESP_LOGE(TAG, "inside rgb_example_wifi_sta_do_disconnect");
    /* Unregisters the handler function rgb_example_handler_on_wifi_disconnect previously
       registered in rgb_example_wifi_sta_do_connect function, after that, check for errors */
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &rgb_example_handler_on_wifi_disconnect));
    /* Unregisters the handler function rgb_example_handler_on_sta_got_ip previously registered in
       rgb_example_wifi_sta_do_connect function and check for errors */
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &rgb_example_handler_on_sta_got_ip));
    /* Unregisters the handler function rgb_example_handler_on_wifi_connect previously registered
       in rgb_example_wifi_sta_do_connect function and check for errors */
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &rgb_example_handler_on_wifi_connect));
#if CONFIG_EXAMPLE_CONNECT_IPV6
    /* Unregisters the handler function rgb_example_handler_on_sta_got_ipv6 previously registered
       in rgb_example_wifi_sta_do_connect function and check for errors */
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &rgb_example_handler_on_sta_got_ipv6));
#endif
    if (s_semph_get_ip_addrs) {
        vSemaphoreDelete(s_semph_get_ip_addrs); // Deletes the IPv4 semaphore if it was created
    }
#if CONFIG_EXAMPLE_CONNECT_IPV6
    if (s_semph_get_ip6_addrs) {
        vSemaphoreDelete(s_semph_get_ip6_addrs); // Deletes the IPv6 semaphore if it was created
    }
#endif
    // Disconnects the WiFi station from the Access Point
    return esp_wifi_disconnect();
}

#endif /* CONFIG_EXAMPLE_CONNECT_WIFI */

void app_main(void)
{
    ESP_LOGE(TAG, "inside app_main");
    /* Step 1: Configures the GPIO for LED outputs and check for errors, terminates the program 
       if returned code is not ESP_OK */
    ESP_ERROR_CHECK(init_led());

    // Step 2: Declare an HTTP Server instance handle as NULL
    static httpd_handle_t server = NULL;

    /* Step 3: Initialize the default NVS partition (this is for storing the wifi credentials in
       flash) and check for errors, terminates the program if returned code is not ESP_OK */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Step 4: Initialize the underlying TCP/IP stack and check for errors, terminates the program
       if returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Step 5: Creates the default event loop and check for errors, terminates the program if
       returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Step 6: Register an event handler to start server when WiFi is connected and check for 
       errors, terminates the program if returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));

    /* Step 7: Register an event handler to stop the server when WiFi is disconnected and check
       for errors, terminates the program if returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    /* Step 8: Configure Wi-Fi or Ethernet, connect, wait for IP and check for errors, terminates
       the program if returned code is not ESP_OK.
        
       This all-in-one helper function is used in protocols examples to
       reduce the amount of boilerplate in the example.
        
       It is not intended to be used in real world applications.
       See examples under examples/wifi/getting_started/ and examples/ethernet/
       for more complete Wi-Fi or Ethernet initialization code.
        
       Read "Establishing Wi-Fi or Ethernet Connection" section in
       examples/protocols/README.md for more information about this function. */
    ESP_ERROR_CHECK(rgb_example_connect()); // defined

}

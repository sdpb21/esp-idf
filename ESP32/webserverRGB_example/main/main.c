#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include <esp_https_server.h>
#include "esp_tls.h"
#include <string.h>
#include "driver/gpio.h"
#include <stdio.h>


#define ledR 33
#define ledG 25
#define ledB 26


int8_t led_r_state = 0;
int8_t led_g_state = 0;
int8_t led_b_state = 0;


static const char *TAG = "main";


esp_err_t init_led(void);
esp_err_t toggle_led(int led);


/* An HTTP GET handler */
// Handler function registered in URI structure, to be called with a supported request method
static esp_err_t root_get_handler(httpd_req_t *req)
{
    /*/ The next 2 lines are useful in case you wish to make a file with binary or text data to be
        available to your component and you don't want to reformat the file as a C source, in our
        case we want to embed the html code stored in the view.html file, we must first specify the
        argument EMBED_TXTFILES in the component registration (CMakeLists.txt file) as follows:
        
        idf_component_register(...
                    EMBED_TXTFILES "view.html")
        
        This will embed the contents of the text file wiew.html as a null-terminated string. The
        file contents will be added to the .rodata section in flash memory, and are available via
        symbol names as can be read in the next 2 lines:*/
    extern unsigned char view_start[] asm("_binary_view_html_start");
    extern unsigned char view_end[] asm("_binary_view_html_end");
    // Get the lenght of the html file stored as a string
    size_t view_len = view_end - view_start;
    char viewHtml[view_len];                // Creates a string with the lenght of the html file
    /*/ Next line copy view_len bytes starting from address pointed by view_start to address 
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
    /*/ Allocates a storage large enough to hold the output (viewHtml) including the terminated 
        null character, returns a pointer to that storage via the first argument (viewHtmlUpdated),
        the pointer should be passed to free to release the allocated storage when it's no longer
        needed. Returns the number of characters written */
    int formattedStrResult = asprintf(&viewHtmlUpdated, viewHtml, led_r_state ? "ON" : "OFF", led_g_state ? "ON" : "OFF", led_b_state ? "ON" : "OFF");

    /*/ Next line sets the 'Content type' field of the HTTP response to text/html, but it isn't 
        sent out until any of the send APIs is executed */
    httpd_resp_set_type(req, "text/html");

    // If the number of characters returned by asprintf is greater than 0, then...
    if (formattedStrResult > 0)
    {
        // Send a complete HTML response, assumes that the entire response is in a buffer (viewHtmlUpdated)
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


    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");

    // Define and initialize an HTTPS server config struct, with default values
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    /*/ Transport mode insecure (SSL disabled) to start the server without SSL, this is used for
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
    // Stop the httpd server
    httpd_ssl_stop(server);
}

// Handler function to stop the webserver when station is disconnected from WiFi
static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
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
    if (*server == NULL)
    {
        *server = start_webserver();
    }
}


esp_err_t init_led(void)
{
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
    /*/ Initializes the configuration structure parameters with default values to be passed to
        esp_wifi_init call */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /** Initializes the Wifi, allocates resources for wifi drivers such as wifi control structure,
     *  RX/TX buffer, wifi NVS structure, etc. Also starts wifi task. This API must be called 
     *  before all other WiFi APIs must be called. You must always use WIFI_INIT_CONFIG_DEFAULT to
     *  set the configuration default values, this guarantee that all the fields get the right
     *  value when more fields are added to wifi_init_config_t structure in a future release. If
     *  you want to set your own initial values, overwrite the default values which are set by
     *  WIFI_INIT_CONFIG_DEFAULT. Please be notified that the field 'magic' of wifi_init_config_t
     *  should always be WIFI_INIT_CONFIG_MAGIC! (THIS IS WEIRD, DIG INTO THIS IN THE FUTURE),
     *  and check for errors */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /*/ Define an ESP-netif (NETwork InterFace) inherent config struct and initializes it with
         default parameters */
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    /*/ Warning: the interface desc is used in tests to capture actual connection details (IP, gw,
         mask) */
    // Change the interface description initialized with ESP_NETIF_INHERENT_DEFAULT_WIFI_STA
    esp_netif_config.if_desc = EXAMPLE_NETIF_DESC_STA;
    /*/ Change the priority of the interface from it's default value setted with ESP_NETIF_INHERENT
        _DEFAULT_WIFI_STA to 128, the higher the value, the higher the priority */
    esp_netif_config.route_prio = 128;
    /*/ Creates esp_netif WiFi object of STATION type, based on the custom configuration and 
        returns a pointer to the esp_netif instance */
    s_example_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    // Set default handlers for station
    esp_wifi_set_default_wifi_sta_handlers();

    // Set the WiFi API configuration storage type, the configuration will be stored in memory
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // Set the WiFi operating mode as station mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /*/ Starts WiFi according to current configuration, creates a station control block and starts
        the station because the mode is station */
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void rgb_example_handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    s_retry_num++;
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
    if (disconn->reason == WIFI_REASON_ROAMING) {
        ESP_LOGD(TAG, "station roaming, do nothing");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected %d, trying to reconnect...", disconn->reason);
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
bool rgb_example_is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static void rgb_example_handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!rgb_example_is_our_netif(EXAMPLE_NETIF_DESC_STA, event->esp_netif)) {  // defined
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}

static void rgb_example_handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
#if CONFIG_EXAMPLE_CONNECT_IPV6
    esp_netif_create_ip6_linklocal(esp_netif);
#endif // CONFIG_EXAMPLE_CONNECT_IPV6
}

static void rgb_example_handler_on_sta_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    if (!rgb_example_is_our_netif(EXAMPLE_NETIF_DESC_STA, event->esp_netif)) {  // defined
        return;
    }
    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
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
    if (wait) {
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
        /* Takes the semaphore token for the IPv4 if it's given, if not, waits a quantity of time
           given in ticks by portMAX_DELAY, for the token to be given */
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
       configured, returns ESP_OK if connection was successful */
    return rgb_example_wifi_sta_do_connect(wifi_config, true);  // defined

}

static esp_err_t rgb_example_wifi_sta_do_disconnect(void)
{
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
        vSemaphoreDelete(s_semph_get_ip_addrs);
    }
#if CONFIG_EXAMPLE_CONNECT_IPV6
    if (s_semph_get_ip6_addrs) {
        vSemaphoreDelete(s_semph_get_ip6_addrs);
    }
#endif
    // Disconnects the WiFi station from the Access Point
    return esp_wifi_disconnect();
}

static void rgb_example_wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_sta_netif));
    esp_netif_destroy(s_example_sta_netif);
    s_example_sta_netif = NULL;
}

void rgb_example_wifi_shutdown(void)
{
    rgb_example_wifi_sta_do_disconnect();   // defined
    rgb_example_wifi_stop();                // defined
}

static void rgb_example_print_all_netif_ips(const char *prefix)
{
    // Print all IPs in TCPIP context to avoid potential races of removing/adding netifs when iterating over the list
    esp_netif_tcpip_exec(print_all_ips_tcpip, (void*) prefix);
}

static esp_err_t rgb_example_connect(void)
{
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

void app_main(void)
{
    /*/ Step 1: Configures the GPIO for LED outputs and check for errors, terminates the program 
        if returned code is not ESP_OK */
    ESP_ERROR_CHECK(init_led());

    // Step 2: Declare an HTTP Server instance handle as NULL
    static httpd_handle_t server = NULL;

    /*/ Step 3: Initialize the default NVS partition (this is for storing the wifi credentials in
        flash) and check for errors, terminates the program if returned code is not ESP_OK */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Step 4: Initialize the underlying TCP/IP stack and check for errors, terminates the program
        if returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Step 5: Creates the default event loop and check for errors, terminates the program if
        returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Step 6: Register an event handler to start server when wifi is connected and check for 
        errors, terminates the program if returned code is not ESP_OK */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));

    /* Step 7: Register an event handler to stop the server when wifi is disconnected and check
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

    /*
esp_err_t example_connect(void)
{
#if CONFIG_EXAMPLE_CONNECT_ETHERNET
    if (example_ethernet_connect() != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&example_ethernet_shutdown));
#endif
#if CONFIG_EXAMPLE_CONNECT_WIFI
    if (example_wifi_connect() != ESP_OK) {
////////////////////////////////////////////////////////////////////////////////////////////////
        esp_err_t example_wifi_connect(void)
        {
            ESP_LOGI(TAG, "Start example_connect.");
            example_wifi_start();
////////////////////////////////////////////////////////////////////////////////////////////////
            void example_wifi_start(void)
            {
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                ESP_ERROR_CHECK(esp_wifi_init(&cfg));

                esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
                // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
                esp_netif_config.if_desc = EXAMPLE_NETIF_DESC_STA;
                esp_netif_config.route_prio = 128;
                s_example_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
                esp_wifi_set_default_wifi_sta_handlers();

                ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                ESP_ERROR_CHECK(esp_wifi_start());
            }

////////////////////////////////////////////////////////////////////////////////////////////////
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
        #if CONFIG_EXAMPLE_WIFI_SSID_PWD_FROM_STDIN
            example_configure_stdin_stdout();
            char buf[sizeof(wifi_config.sta.ssid)+sizeof(wifi_config.sta.password)+2] = {0};
            ESP_LOGI(TAG, "Please input ssid password:");
            fgets(buf, sizeof(buf), stdin);
            int len = strlen(buf);
            buf[len-1] = '\0'; //* removes '\n'
            memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));

            char *rest = NULL;
            char *temp = strtok_r(buf, " ", &rest);
            strncpy((char*)wifi_config.sta.ssid, temp, sizeof(wifi_config.sta.ssid));
            memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
            temp = strtok_r(NULL, " ", &rest);
            if (temp) {
                strncpy((char*)wifi_config.sta.password, temp, sizeof(wifi_config.sta.password));
            } else {
                wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
            }
        #endif
            return example_wifi_sta_do_connect(wifi_config, true);
////////////////////////////////////////////////////////////////////////////////////////////////
            esp_err_t example_wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
            {
                if (wait) {
                    s_semph_get_ip_addrs = xSemaphoreCreateBinary();
                    if (s_semph_get_ip_addrs == NULL) {
                        return ESP_ERR_NO_MEM;
                    }
            #if CONFIG_EXAMPLE_CONNECT_IPV6
                    s_semph_get_ip6_addrs = xSemaphoreCreateBinary();
                    if (s_semph_get_ip6_addrs == NULL) {
                        vSemaphoreDelete(s_semph_get_ip_addrs);
                        return ESP_ERR_NO_MEM;
                    }
            #endif
                }
                s_retry_num = 0;
                ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect, NULL));
                ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &example_handler_on_sta_got_ip, NULL));
                ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &example_handler_on_wifi_connect, s_example_sta_netif));
            #if CONFIG_EXAMPLE_CONNECT_IPV6
                ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &example_handler_on_sta_got_ipv6, NULL));
            #endif

                ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                esp_err_t ret = esp_wifi_connect();
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
                    return ret;
                }
                if (wait) {
                    ESP_LOGI(TAG, "Waiting for IP(s)");
            #if CONFIG_EXAMPLE_CONNECT_IPV4
                    xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
            #endif
            #if CONFIG_EXAMPLE_CONNECT_IPV6
                    xSemaphoreTake(s_semph_get_ip6_addrs, portMAX_DELAY);
            #endif
                    if (s_retry_num > CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY) {
                        return ESP_FAIL;
                    }
                }
                return ESP_OK;
            }

////////////////////////////////////////////////////////////////////////////////////////////////
        }
////////////////////////////////////////////////////////////////////////////////////////////////
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&example_wifi_shutdown));
#endif
#if CONFIG_EXAMPLE_CONNECT_THREAD
    if (example_thread_connect() != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&example_thread_shutdown));
#endif
#if CONFIG_EXAMPLE_CONNECT_PPP
    if (example_ppp_connect() != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&example_ppp_shutdown));
#endif

#if CONFIG_EXAMPLE_CONNECT_ETHERNET
    example_print_all_netif_ips(EXAMPLE_NETIF_DESC_ETH);
#endif

#if CONFIG_EXAMPLE_CONNECT_WIFI
    example_print_all_netif_ips(EXAMPLE_NETIF_DESC_STA);
#endif

#if CONFIG_EXAMPLE_CONNECT_THREAD
    example_print_all_netif_ips(EXAMPLE_NETIF_DESC_THREAD);
#endif

#if CONFIG_EXAMPLE_CONNECT_PPP
    example_print_all_netif_ips(EXAMPLE_NETIF_DESC_PPP);
#endif

    return ESP_OK;
}
*/
}

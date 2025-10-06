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

// Handler function to stop the webserver when wifi is disconnected
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

// Handler function that is called to start the webserver when wifi is connected
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
    ESP_ERROR_CHECK(example_connect());
}

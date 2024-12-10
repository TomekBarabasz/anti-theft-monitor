#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <common.h>
#include <controller.h>
#include <string>
#include <memory>
#include <atomic>

#define KEEPALIVE_IDLE              30
#define KEEPALIVE_INTERVAL          30
#define KEEPALIVE_COUNT             10

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#if defined(CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK)
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif defined(CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT)
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif defined(CONFIG_ESP_WPA3_SAE_PWE_BOTH)
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

namespace {
constexpr int DEFAULT_TCP_PORT = 5555;
constexpr int DEFAULT_UDP_PORT = 6666;
const char *TAG = "WIFI";

enum class WifiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
};

[[maybe_unused]] int initialize_tcp_socket_ipv6(int port)
{
    struct sockaddr_storage dest_addr;
    struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
    bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
    dest_addr_ip6->sin6_family = AF_INET6;
    dest_addr_ip6->sin6_port = htons(port);

    int listen_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    opt = 0;
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    return listen_sock;
    
CLEAN_UP:
    close(listen_sock);
    return -1;
}

class WifiHelper
{
    /* FreeRTOS event group to signal when we are connected*/
    inline static EventGroupHandle_t s_wifi_event_group;
    inline static int s_retry_num;
    static constexpr int WIFI_CONNECT_MAXIMUM_RETRY = 3;
    static constexpr int WIFI_CHANNEL = 1;
    static constexpr int MAX_STA_CONNECTIONS = 1;

    static void event_handler_sta(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_retry_num < WIFI_CONNECT_MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP");
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(TAG,"connect to the AP fail");
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
    static void event_handler_ap(void* arg, esp_event_base_t event_base,
                                        int32_t event_id, void* event_data)
    {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
        }
    }

    static bool init_sta(wifi_config_t & config)
    {
        s_retry_num = 0;
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler_sta,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler_sta,
                                                            NULL,
                                                            &instance_got_ip));
        /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
                * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
                * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
                * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
                */
        config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        //config.sta.sae_pwe_h2e = ESP_WIFI_SAE_MODE,
        //config.sta.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        
        //use ram instad of flash ? 
        //ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_sta finished.");

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
        * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
        * happened. */
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "connected to ap SSID:%s", config.sta.ssid);
            return true;
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s", config.sta.ssid);
            return false;
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
            return false;
        }
    }

    static bool init_ap(wifi_config_t & config)
    {
        esp_netif_create_default_wifi_ap();
         wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler_ap,
                                                            NULL,
                                                            NULL));
        config.ap.ssid_len = strlen(reinterpret_cast<const char*>(config.ap.ssid));
        config.ap.channel = WIFI_CHANNEL;
        config.ap.max_connection = MAX_STA_CONNECTIONS;
        config.ap.authmode = WIFI_AUTH_WPA2_PSK,
        config.ap.pmf_cfg = { true, true };

        if (strlen(reinterpret_cast<const char*>(config.ap.password)) == 0) {
            config.ap.authmode = WIFI_AUTH_OPEN;
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s channel:%d", config.ap.ssid, WIFI_CHANNEL);
        return true;
    }

public:
    static bool init(WifiMode mode, wifi_config_t & config)
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        if (mode == WifiMode::STA) {
            return init_sta(config);
        } else {
            return init_ap(config);
        }
    }

    static void deinit()
    {
        esp_err_t err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            return;
        }
        ESP_ERROR_CHECK(err);
        ESP_ERROR_CHECK(esp_wifi_deinit());
        // C:\tomek\projects\from-github\esp-idf\examples\common_components\protocol_examples_common\wifi_connect.c
        // ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_sta_netif));
        // esp_netif_destroy(s_example_sta_netif);
        // s_example_sta_netif = NULL;
    }
};

struct TcpController : public Controller
{
    TcpController(esp_event_loop_handle_t el, const evStartTcpControllerParams& prms) : 
        port(prms.port),
        wifi_mode(prms.wifi_mode),
        event_loop(el)
    {
        wifi_config = std::make_unique<wifi_config_t>();
        if (wifi_mode == WifiMode::STA) {
            memcpy(wifi_config->sta.ssid,     prms.ssid,     sizeof(prms.ssid));
            memcpy(wifi_config->sta.password, prms.password, sizeof(prms.password));
        } else {
            memcpy(wifi_config->ap.ssid,     prms.ssid,     sizeof(prms.ssid));
            memcpy(wifi_config->ap.password, prms.password, sizeof(prms.password));
        }        
    }
    bool run() override
    {
        if (wifi_state == WifiState::DISCONNECTED) {
            wifi_state = WifiState::CONNECTING;
            xTaskCreate(connect_wifi,"connect-wifi-task", 4096, this,  5, nullptr);
            return true;
        }
        return false;
    }
    bool run_tcp_server()
    {
        tcp_server_running = true;
        listen_sock = initialize_tcp_socket_ipv4(port);
        if (listen_sock < 0) {
            return false;
        }
        xTaskCreate(server_main, "tcp-server",   4096, this,  5, nullptr);
        return true;
    }
    void stop() override
    {
        tcp_server_running = false;
        wifi_state = WifiState::DISCONNECTED;
    }
    void release() override
    {
        stop();
        delete this;
    }
    void handle_incomming_data(int sock)
    {
        uint8_t rx_buffer[512];
        int nbytes;
        do
        {
            nbytes = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
            if (nbytes < 0) {
                ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
            } else if (nbytes == 0) {
                ESP_LOGW(TAG, "Connection closed");
            } else 
            {
                ESP_ERROR_CHECK(esp_event_post_to(event_loop, EXTERNAL_COMMAND_EVENTS, 0, rx_buffer, nbytes, portMAX_DELAY));
            }
        } while(nbytes > 0 && tcp_server_running);
    }
    static void connect_wifi(void *params)
    {
        auto & inst = *static_cast<TcpController*>(params);
        const auto res = WifiHelper::init(inst.wifi_mode, *inst.wifi_config);
        if (res) {
            inst.wifi_state = WifiState::CONNECTED;
            inst.run_tcp_server();
            inst.wifi_config.reset();
        }
        vTaskDelete(nullptr);
    }
    static void server_main(void *params)
    {
        auto & inst = *static_cast<TcpController*>(params);

        for(;inst.tcp_server_running;)
        {
            int sock = connect_tcp_socket(inst.listen_sock);
            if (sock < 0) continue;
            inst.handle_incomming_data(sock);
            shutdown(sock, 0);
            close(sock);
        }

        close(inst.listen_sock);
        inst.listen_sock = -1;
        WifiHelper::deinit();
        vTaskDelete(NULL);
    }
    static int initialize_tcp_socket_ipv4(int port)
    {
        struct sockaddr_storage dest_addr;
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(port);

        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listen_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            return -1;
        }
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ESP_LOGI(TAG, "Socket created");

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to bind AF_INET : errno %d", errno);
            goto CLEAN_UP;
        }
        ESP_LOGI(TAG, "Socket bound, port %d", port);

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            goto CLEAN_UP;
        }

        return listen_sock;
        
    CLEAN_UP:
        close(listen_sock);
        vTaskDelete(NULL);
        return -1;
    }
    static int connect_tcp_socket(int listen_sock)
    {
        const int keepAlive = 1;
        const int keepIdle = KEEPALIVE_IDLE;
        const int keepInterval = KEEPALIVE_INTERVAL;
        const int keepCount = KEEPALIVE_COUNT;

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            return -1;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET,  SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        
        // Convert ip address to string
        char addr_str[128];
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        else if (source_addr.ss_family == PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
        return sock;
    }

    const int port;
    const WifiMode wifi_mode;
    int listen_sock {-1};
    esp_event_loop_handle_t event_loop {nullptr};
    WifiState wifi_state {WifiState::DISCONNECTED};
    std::unique_ptr<wifi_config_t> wifi_config {nullptr};
    std::atomic<bool> tcp_server_running;
};
}

Controller* start_tcp_controller(esp_event_loop_handle_t event_loop, const evStartTcpControllerParams& prms)
{
    return new TcpController(event_loop,prms);
}


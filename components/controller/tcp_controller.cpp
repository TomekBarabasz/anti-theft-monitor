#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <common.h>
#include <controller.h>

#define PORT                        1234
#define KEEPALIVE_IDLE              30
#define KEEPALIVE_INTERVAL          30
#define KEEPALIVE_COUNT             10
#define CONFIG_EXAMPLE_IPV4

namespace {
constexpr int DEFAULT_TCP_PORT = 5555;
constexpr int DEFAULT_UDP_PORT = 6666;
const char *TAG = "WIFI";

int initialize_tcp_socket_ipv4(int port)
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
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

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

int connect_tcp_socket(int listen_sock)
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
}

struct TcpController : public Controller
{
    TcpController(esp_event_loop_handle_t el, int port) : event_loop(el)
    {
        listen_sock = initialize_tcp_socket_ipv4(port);
    }
    bool run() override
    {
        if (listen_sock < 0) {
            return false;
        }
        xTaskCreate(task_main,"tcp-server",   4096, event_loop,  5, &task_handle);
        return true;
    }
    void stop() override
    {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
    void release() override
    {
        if (task_handle) {
            stop();
        }
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
        } while(nbytes > 0);
    }

    static void task_main(void *params)
    {
        auto inst = *static_cast<TcpController*>(params);

        for(;;)
        {
            int sock = connect_tcp_socket(inst.listen_sock);
            if (sock < 0) continue;
            inst.handle_incomming_data(sock);
            shutdown(sock, 0);
            close(sock);
        }

        close(inst.listen_sock);
        inst.listen_sock = -1;
        vTaskDelete(NULL);
    }

    int listen_sock {-1};
    esp_event_loop_handle_t event_loop {nullptr};
    TaskHandle_t task_handle {nullptr};
};

Controller* start_tcp_controller(esp_event_loop_handle_t event_loop, const evStartTcpControllerParams*)
{
    return new TcpController(event_loop,DEFAULT_TCP_PORT);
}


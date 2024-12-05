#include <esp_log.h>
#include <esp_event.h>

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include <event_monitor.h>
#include <regex>
#include <common.h>

const char *TAG = "EVMON";

namespace {
EventMonitor* monitor_instance {nullptr};
}
EventMonitor* EventMonitor::get_instance() { return monitor_instance;}

struct UdpEventMonitor : public EventMonitor
{
    UdpEventMonitor(const char* ip_address, int port)
    {
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            ESP_LOGE(TAG, "Socket creation failed %s", strerror(errno));
            return;
        }
        
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip_address, &dest_addr.sin_addr) <= 0) {
            ESP_LOGE(TAG, "inet_pton failed %s", strerror(errno));
            close(sockfd);
            sockfd = -1;
        }
        event_loop = create_event_loop();
        ESP_ERROR_CHECK(esp_event_handler_instance_register_with(event_loop, ANTITHEFT_APP_EVENTS, ESP_EVENT_ANY_ID, UdpEventMonitor::event_handler, this, &event_handler_instance));
    }
    bool send(std::string&& event) override
    {
        auto * ev = new std::string( std::move(event) );
        return esp_event_post_to(event_loop, ANTITHEFT_APP_EVENTS, 0, ev, ev->size(), portMAX_DELAY) == ESP_OK;
    }
    bool good() const { return sockfd > 0; }
    explicit operator bool() const { return good(); }

    void release() override 
    {
        close(sockfd);
        sockfd = -1;
        esp_event_handler_instance_unregister_with(event_loop,ANTITHEFT_APP_EVENTS, ESP_EVENT_ANY_ID,event_handler_instance);
        esp_event_loop_delete(event_loop);
        event_loop = nullptr;
        monitor_instance = nullptr;
        delete this;
    }
protected:
    esp_event_loop_handle_t create_event_loop()
    {
        esp_event_loop_args_t loop_args = {
            .queue_size = 4,
            .task_name = "event_monitor_event_loop",
            .task_priority = 5, //uxTaskPriorityGet(NULL),
            .task_stack_size = 1024,
            .task_core_id = 1
        };
        esp_event_loop_handle_t loop_handle;
        esp_event_loop_create(&loop_args, &loop_handle);
        return loop_handle;
    }
    static void event_handler(void* args, esp_event_base_t base, int32_t id, void* event_data_raw)
    {
        auto * inst = static_cast<UdpEventMonitor*>(args);
        auto * event = static_cast<std::string*>(event_data_raw);
        inst->udp_send(*event);
        delete event;
    }
    bool udp_send(const std::string& event)
    {
        if (sendto(sockfd, event.c_str(), event.size(), 0, (const struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            ESP_LOGE(TAG, "sendto failed %s", strerror(errno));
            return false;
        } else {
            return true;
        }
    }
    int sockfd {-1};
    struct sockaddr_in dest_addr;
    esp_event_loop_handle_t event_loop {nullptr};
    esp_event_handler_instance_t event_handler_instance {nullptr};
};

EventMonitor* EventMonitor::create_instance(const char* type)
{
    std::regex udp(R"(udp:([^:]+):(\d+))");
    std::smatch matches;
    std::string _type {type};
    if (std::regex_match(_type, matches, udp)) 
    {
        const std::string& ip_address = matches[1].str();
        int port = std::stoi(matches[2].str());
        auto inst = new UdpEventMonitor(ip_address.c_str(),port);
        if (*inst) {
            monitor_instance = inst;
            return inst;
        } else {
            delete inst;
        }
    }
    return nullptr;
}
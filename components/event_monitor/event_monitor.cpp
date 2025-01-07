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
    UdpEventMonitor(const CmdStartUdpMonitor& prms)
    {
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            ESP_LOGE(TAG, "Socket creation failed %s", strerror(errno));
            return;
        }
        
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(prms.port);

        char ip_address[16];
        snprintf(ip_address, sizeof(ip_address), "%d.%d.%d.%d", prms.ip[0], prms.ip[1], prms.ip[2], prms.ip[3]);
        ip_address[15] = '\0';
        if (inet_pton(AF_INET, ip_address, &dest_addr.sin_addr) <= 0) {
            ESP_LOGE(TAG, "inet_pton failed %s", strerror(errno));
            close(sockfd);
            sockfd = -1;
        }
    }
    bool send(EventMonitor::Severity s, const char* tag, std::string&& event) override
    {
        auto msg = fmt::format("{}:{}",tag,event);
        if (sendto( sockfd, msg.c_str(), msg.size(), 0, 
                    (const struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            ESP_LOGE(TAG, "sendto failed %s", strerror(errno));
            return false;
        }
        return true;
    }
    void release() override 
    {
        close(sockfd);
        sockfd = -1;
        monitor_instance = nullptr;
        delete this;
    }
    int sockfd {-1};
    struct sockaddr_in dest_addr;
};

struct SerialEventMonitor : public EventMonitor
{
    bool send(EventMonitor::Severity s, const char* tag, std::string&& event) override
    {
        switch (s) {
            case Severity::debug:
            case Severity::info:
                ESP_LOGI(tag, "%s", event.c_str());
                break;
            case Severity::error:
                ESP_LOGE(tag, "%s", event.c_str());
                break;
        }
        return true;
    }
    void release() override
    {
        delete this;
    }
};

EventMonitor* EventMonitor::create_instance(const char* type, void *prms_)
{
    if (0 == strcmp(type,"udp")) {
        auto & prms = *reinterpret_cast<const CmdStartUdpMonitor*>(prms_);
        monitor_instance = new UdpEventMonitor(prms);
        return monitor_instance;
    } else if (0 == strcmp(type,"serial")) {
        monitor_instance = new SerialEventMonitor();
        return monitor_instance;
    } else {
        return nullptr;
    }
}
#include <event_monitor.h>
#include <esp_log.h>

namespace {
struct SerialMonitor : public EventMonitor
{
    int start() override
    {
        return 1;
    }
    int send(const char* data, int len) override
    {
        ESP_LOGE("EWM","%s",data);
        return 1;
    }
    void stop() override
    {
    }
};
}

void EventMonitor::create_serial()
{
    if (p_instance) {
        delete p_instance;
    }
    p_instance = new SerialMonitor();
}

#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include <common.h>
#include <controller.h>

extern Controller* start_tcp_controller(esp_event_loop_handle_t event_loop, const evStartTcpControllerParams*);
extern Controller* start_gprs_controller(esp_event_loop_handle_t event_loop);
//extern Controller* start_bluetooth_controller(esp_event_loop_handle_t event_loop);

Controller* Controller::create_instance(esp_event_loop_handle_t event_loop,const char* type, void* params_)
{ 
    if (0 == strcmp(type,"gprs")) {
        return start_gprs_controller(event_loop);
    }
    else if (0 == strcmp(type,"tcp")) {
        auto * params = reinterpret_cast<const evStartTcpControllerParams*>(params_);
        return start_tcp_controller(event_loop, params);
    }
    /*else if (0 == strcmp(type,"bluetooth")) {
        return start_bluetooth_controller(event_loop);
    }*/

    ESP_LOGE("SRV","Invalid type specified %s",type);
    return nullptr;
}
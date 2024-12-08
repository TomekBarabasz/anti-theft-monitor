#include "esp_event.h"
#include <controller.h>

struct GprsController : public Controller
{
    GprsController(esp_event_loop_handle_t el) : event_loop(el)
    {

    }
    bool run() override
    {
        return false;
    }
    void stop() override
    {
        
    }
    void release() override
    {
        
    }

    esp_event_loop_handle_t event_loop {nullptr};
};

Controller* start_gprs_controller(esp_event_loop_handle_t event_loop)
{
    return new GprsController(event_loop);
}

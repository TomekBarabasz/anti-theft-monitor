#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include <common.h>

namespace {
}

namespace AntiTheftMonitor 
{
extern "C" void pir_monitor_task(void* params)
{
    auto event_loop = reinterpret_cast<esp_event_loop_handle_t>(params); 
    vTaskDelete(NULL);
}
}
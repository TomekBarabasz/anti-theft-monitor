#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"

#include <server.h>
#include <motion_detector.h>
#include <gprs_modem.h>

namespace AntiTheftMonitor
{
extern "C" void pir_monitor_task(void* params);
}

namespace {
esp_event_loop_handle_t create_main_event_loop()
{
    esp_event_loop_args_t loop_args = {
        .queue_size = 8,
        .task_name = "main_event_loop",
        .task_priority = 5, //uxTaskPriorityGet(NULL),
        .task_stack_size = 2048,
        .task_core_id = 1
    };
    esp_event_loop_handle_t loop_handle;
    esp_event_loop_create(&loop_args, &loop_handle);
    return loop_handle;
}
}
using namespace AntiTheftMonitor;

Server * srv {nullptr};
MotionDetector * motd {nullptr};
GprsModem *modem {nullptr};
esp_event_loop_handle_t main_event_loop {nullptr};

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    main_event_loop = create_main_event_loop();
    srv = Server::create_instance(main_event_loop,"tcp");
    motd = MotionDetector::create_instance(main_event_loop);
    modem = GprsModem::create_instance(main_event_loop);

    srv->run();
    motd->run();
    modem->run();
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"

#include <controller.h>
#include <motion_detector.h>
#include <spray_releaser.h>
#include <common.h>

namespace {
static const char* TAG="MAIN";
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
class AntitheftApp 
{
    public:
    AntitheftApp()
    {
        main_event_loop = create_main_event_loop();
        ESP_ERROR_CHECK(esp_event_handler_instance_register_with(main_event_loop, EXTERNAL_COMMAND_EVENTS, ESP_EVENT_ANY_ID, external_cmds_handler,   this, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register_with(main_event_loop, HARDWARE_BASED_EVENTS,   ESP_EVENT_ANY_ID, hardware_events_handler, this, NULL));

        sprayReleaser = SprayReleaser::create_instance();
        motd = MotionDetector::create_instance(main_event_loop);
        gprsCtrl = Controller::create_instance(main_event_loop,"gprs");
    }
    ~AntitheftApp()
    {
        motd->stop();
        sprayReleaser->stop();
        gprsCtrl->stop();
        
        motd->release();
        sprayReleaser->release();
        gprsCtrl->release();
    }
    bool run()
    {
        return sprayReleaser->run() && motd->run() && gprsCtrl->run();
    }
    esp_event_loop_handle_t get_loop_handle() { return main_event_loop; }
protected:
    static void external_cmds_handler(void* args, esp_event_base_t evBase, int32_t evId, void* evData)
    {
        auto & inst = *reinterpret_cast<AntitheftApp*>(args);
        switch(evId)
        {
            case evAuthenticate:
            case evDisarmSprayReleaser:
            case evArmSprayReleaser:
            case evFlashOn:
            case evFlashOff:
            case evFlashBlink:
            case evStartVoiceCall:
            case evEndVoiceCall:
            case evCapturePhoto:
            case evStartStreaming:
            case evPairBluetooth:
            case evStartTcpController:
                if (inst.tcpCtrl != nullptr) {
                    inst.tcpCtrl->stop();
                    inst.tcpCtrl->release();
                }
                inst.tcpCtrl = Controller::create_instance(inst.main_event_loop, "tcp", evData);
                inst.tcpCtrl->run();
                break;
            case evStartBluetooth:
            default:;
        }
    }
    static void hardware_events_handler(void* args, esp_event_base_t evBase, int32_t evId, void* evData)
    {
        auto & inst = *reinterpret_cast<AntitheftApp*>(args);
        ESP_LOGI(TAG, "hardware evnent %ld received",evId);
        switch(evId) 
        {
        case evMotionDetected:
            inst.sprayReleaser->activate();
            break;
        }
    }
    esp_event_loop_handle_t main_event_loop {nullptr};
    MotionDetector *motd {nullptr};
    SprayReleaser *sprayReleaser {nullptr};
    Controller *gprsCtrl {nullptr};
    Controller *tcpCtrl {nullptr};
    Controller *bluetoothCtrl {nullptr};
};
}

AntitheftApp *app {nullptr};


extern "C" void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    app = new AntitheftApp();
    app->run();
}

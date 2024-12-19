#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"

//for keepalive task
#include "driver/gpio.h"

#include <common.h>
#include <controller.h>
#include <motion_detector.h>
#include <spray_releaser.h>
#include <event_monitor.h>

#include <string.h>
#include <chrono>
#include <credentials.h>

using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;
using CLK = std::chrono::steady_clock;

namespace {
static const char* TAG="MAIN";
esp_event_loop_handle_t create_main_event_loop()
{
    esp_event_loop_args_t loop_args = {
        .queue_size = 8,
        .task_name = "main_event_loop",
        .task_priority = 5, //uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
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
        hw_ev_tm = CLK::now();
        return sprayReleaser->run() && motd->run() && gprsCtrl->run();
    }
    esp_event_loop_handle_t get_loop_handle() { return main_event_loop; }
protected:
    static void external_cmds_handler(void* args, esp_event_base_t evBase, int32_t evId, void* evData)
    {
        auto & inst = *reinterpret_cast<AntitheftApp*>(args);
        ESP_LOGI(TAG, "external cmd  %ld received",evId);
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
            case evStartAudioStreaming:
            case evStartVideoStreaming:
            case evPairBluetooth:
                break;
            case evStartTcpController:
                if (nullptr == inst.tcpCtrl) {
                    inst.tcpCtrl = Controller::create_instance(inst.main_event_loop, "tcp", evData);
                    inst.tcpCtrl->run();
                }
                break;
            case evTcpControllerStopped:
                if (inst.tcpCtrl != nullptr) {
                    inst.tcpCtrl->release();
                    inst.tcpCtrl = nullptr;
                }
                break;
            case evStartBluetooth:
                break;
            case evStartUpdMonitor:{
                auto & prms = *reinterpret_cast<const CmdStartUdpMonitor*>(evData);
                ESP_LOGI(TAG,"evStartUpdMonitor received ip %d.%d.%d.%d port %d",
                    prms.ip[0],prms.ip[1],prms.ip[2],prms.ip[3],prms.port);
                if (nullptr == inst.evMon){
                    inst.evMon = EventMonitor::create_instance("udp",evData);
                }
                break;}
            case evStopUpdMonitor:
                if (inst.evMon != nullptr) {
                    inst.evMon->release();
                    inst.evMon = nullptr;
                }
                break;
            case evEcho:
                if (inst.evMon != nullptr) {
                    auto & prms = *reinterpret_cast<const CmdEcho*>(evData);
                    std::string message(prms.message,prms.n_chars);
                    inst.evMon->send(EventMonitor::Severity::debug, std::move(message));
                }
                break;
            default:;
        }
    }
    static void hardware_events_handler(void* args, esp_event_base_t evBase, int32_t evId, void* evData)
    {
        auto & inst = *reinterpret_cast<AntitheftApp*>(args);
        ESP_LOGI(TAG, "hardware event %ld received",evId);
        auto tm = CLK::now();
        const std::chrono::duration<float> elapsed_seconds{tm - inst.hw_ev_tm};
        if (elapsed_seconds.count() > 1.0f) 
        {
            switch(evId) 
            {
            case evMotionDetected:
                //inst.sprayReleaser->activate();
                if (nullptr == inst.tcpCtrl) {
                    CmdStartTcpController prms;
                    prms.port = 1234;
                    prms.wifi_mode = WifiMode::STA;
                    strncpy(prms.ssid,      STA_SSID, sizeof(prms.ssid));
                    strncpy(prms.password,  STA_PASSW,   sizeof(prms.password));
                    inst.tcpCtrl = Controller::create_instance(inst.main_event_loop, "tcp", &prms);
                    inst.tcpCtrl->run();
                } else {
                    inst.tcpCtrl->stop();
                }
                break;
            case evBtnPressed_1:
                if (nullptr == inst.tcpCtrl) {
                    CmdStartTcpController prms;
                    prms.port = 1234;
                    prms.wifi_mode = WifiMode::AP;
                    strncpy(prms.ssid,      AP_SSID, sizeof(prms.ssid));
                    strncpy(prms.password,  AP_PASSW,     sizeof(prms.password));
                    inst.tcpCtrl = Controller::create_instance(inst.main_event_loop, "tcp", &prms);
                    inst.tcpCtrl->run();
                } else {
                    inst.tcpCtrl->stop();
                }
                break;
            }
        } else {
            ESP_LOGI(TAG,"ignoring event due to debouncing dt %f",elapsed_seconds.count());
        }
        inst.hw_ev_tm = tm;
    }
    esp_event_loop_handle_t main_event_loop {nullptr};
    MotionDetector *motd {nullptr};
    SprayReleaser *sprayReleaser {nullptr};
    Controller *gprsCtrl {nullptr};
    Controller *tcpCtrl {nullptr};
    Controller *bluetoothCtrl {nullptr};
    EventMonitor *evMon {nullptr};
    timestamp_t hw_ev_tm;
};
}

AntitheftApp *app {nullptr};


 void keep_alive_task(void*)
{
    constexpr auto BLINK_GPIO = static_cast<gpio_num_t>(2);
    constexpr TickType_t CONFIG_BLINK_PERIOD = 1000;
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    
    uint8_t s_led_state = 0;
    for(;;) {
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

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

    xTaskCreate(keep_alive_task, "keep_alive_task", 2048, nullptr, 10, nullptr);
}

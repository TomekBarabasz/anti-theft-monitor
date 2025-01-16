#include <event_monitor.h>
#include <controller.h>
#include <chrono>
#include <string.h>
#include "nvs_flash.h"
#include <credentials.h>
#include <events.h>
#include <commands.h>
#include <motion_detector.h>
#include <spray_releaser.h>

using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;
using CLK = std::chrono::steady_clock;

#define LOG_I(fmt,...)
#define LOG_E(fmt,...)

namespace {
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
        gprsCtrl = create_gprs_controller(main_event_loop);
    }
    ~AntitheftApp()
    {
        motd->stop();
        sprayReleaser->stop();
        gprsCtrl->stop();
        
        MotionDetector::delete_instance(motd);
        SprayReleaser::delete_instance(sprayReleaser);
        Controller::delete_instance(gprsCtrl);
        motd = nullptr;
        sprayReleaser =  nullptr;
        gprsCtrl = nullptr;
    }
    bool run()
    {
        hw_ev_tm = CLK::now();
        return sprayReleaser->run() && motd->start() && gprsCtrl->start();
    }
    esp_event_loop_handle_t get_loop_handle() { return main_event_loop; }
protected:
    static void external_cmds_handler(void* args, esp_event_base_t evBase, int32_t evId, void* evData)
    {
        auto & inst = *reinterpret_cast<AntitheftApp*>(args);
        LOG_I("external cmd {} received",evId);
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
                    inst.tcpCtrl = create_tcp_controller(inst.main_event_loop, *(CmdStartTcpController*)evData);
                    inst.tcpCtrl->start();
                }
                break;
            case evTcpControllerStopped:
                if (inst.tcpCtrl != nullptr) {
                    Controller::delete_instance(inst.tcpCtrl);
                    inst.tcpCtrl = nullptr;
                }
                break;
            case evStartBluetooth:
                break;
            case evStartUpdMonitor:{
                auto & prms = *reinterpret_cast<const CmdStartUdpMonitor*>(evData);
                LOG_I("evStartUpdMonitor received ip {}.{}.{}.{} port {}",
                    prms.ip[0],prms.ip[1],prms.ip[2],prms.ip[3],prms.port);
                EventMonitor::create_udp(prms);
                break;}
            case evStopUpdMonitor:
                EventMonitor::create_serial();
                break;
            case evEcho:{
                auto & prms = *reinterpret_cast<const CmdEcho*>(evData);
                EventMonitor::get_instance()->send(prms.message, prms.n_chars);
                break;}
            case evStartGprsController:
                if (nullptr == inst.gprsCtrl) {
                    inst.gprsCtrl = create_gprs_controller(inst.main_event_loop);
                    if (!inst.gprsCtrl->start()) {
                        Controller::delete_instance(inst.gprsCtrl);
                        inst.gprsCtrl = nullptr;
                        LOG_E("GprsController run failed");
                    }
                }
            default:;
        }
    }
    static void hardware_events_handler(void* args, esp_event_base_t evBase, int32_t evId, void* evData)
    {
        auto & inst = *reinterpret_cast<AntitheftApp*>(args);
        LOG_I("hardware event {} received",evId);
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
                    inst.tcpCtrl = create_tcp_controller(inst.main_event_loop, prms);
                    inst.tcpCtrl->start();
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
                    inst.tcpCtrl = create_tcp_controller(inst.main_event_loop, prms);
                    inst.tcpCtrl->start();
                } else {
                    inst.tcpCtrl->stop();
                }
                break;
            }
        } else {
            LOG_I("ignoring event due to debouncing dt {}",elapsed_seconds.count());
        }
        inst.hw_ev_tm = tm;
    }
    esp_event_loop_handle_t main_event_loop {nullptr};
    MotionDetector *motd {nullptr};
    SprayReleaser *sprayReleaser {nullptr};
    Controller *gprsCtrl {nullptr};
    Controller *tcpCtrl {nullptr};
    Controller *bluetoothCtrl {nullptr};
    timestamp_t hw_ev_tm;
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

    EventMonitor::create_serial();    
    app = new AntitheftApp();
    app->run();
    /*
    CmdStartUdpMonitor cmd {{1,2,3,4},5};
    auto *em = create_udp_event_monitor(cmd);
    em->start();
    const char event[] = "test event";
    em->send(event,sizeof(event));
    em->stop();
    EventMonitor::delete_instance(em);

    esp_event_loop_handle_t event_loop = create_main_event_loop();
    
    CmdStartTcpController cmd1 {1,WifiMode::AP, "dupa", "123"};
    auto *ctrl = create_tcp_controller(event_loop, cmd1);
    ctrl->start();
    ctrl->stop();
    Controller::delete_instance(ctrl);
    */
}

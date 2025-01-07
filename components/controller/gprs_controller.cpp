#include "esp_event.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <event_monitor.h>
#include <controller.h>
#include <fmt/core.h>
#include <atomic>
#include <common.h>


#define ENABLE_GPRS_TRACE
#ifdef ENABLE_GPRS_TRACE
 #define LOG_E(fmts,...) _LOG_E_("GPRS", fmts, ##__VA_ARGS__)
 #define LOG_I(fmts,...) _LOG_I_("GPRS", fmts, ##__VA_ARGS__)
#else
 #define LOG_E(fmt,...)
 #define LOG_I(fmt,...)
#endif

struct GprsController : public Controller
{
    static constexpr uart_port_t uart_port { UART_NUM_0 };
    static constexpr int buff_size = 256;
    GprsController(esp_event_loop_handle_t el) : event_loop(el)
    {
    }
    bool run() override
    {
        uart_config_t uart_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,   //only to disable warning
            .source_clk = UART_SCLK_DEFAULT,
            .flags = 0
        };
        int intr_alloc_flags = 0;

        if (auto err = uart_driver_install(uart_port, buff_size * 2, 0, 0, NULL, intr_alloc_flags); err != ESP_OK) {
            LOG_E("uart_driver_install failed with err : {}", err);
            return false;
        }
        if (auto err = uart_param_config(uart_port, &uart_config); err != ESP_OK) {
            LOG_E("uart_param_config failed with err : {}", err);
            return false;
        }
        if(auto err = uart_set_pin(uart_port, 1, 3, -1, -1); err != ESP_OK) {
            LOG_E("uart_set_pin failed with err : {}", err);
            return false;
        }
        main_task_running = true;
        xTaskCreate(task_main, "motion_detector_task", 1024, this, 10, nullptr);
        return true;
    }
    void stop() override
    {
        main_task_running = false;
    }
    void release() override
    {
        delete this;
    }
    static void task_main(void *params)
    {
        auto & inst = *static_cast<GprsController*>(params);
        while (inst.main_task_running)
        {
            int len = uart_read_bytes(uart_port, inst.rx_buff, buff_size - 1, portMAX_DELAY);
            if (len > 0) {
                inst.rx_buff[len] = '\0';
                LOG_I("received {}",(char*)inst.rx_buff);
            }
        }
        if (auto err = esp_event_post_to(inst.event_loop, EXTERNAL_COMMAND_EVENTS, evGprsControllerStopped, nullptr, 0, portMAX_DELAY); err != ESP_OK) {
            LOG_E("esp_event_post_to failed with err : {}",err);
        }
        vTaskDelete(nullptr);
    }
    uint8_t rx_buff[ buff_size ];
    esp_event_loop_handle_t event_loop {nullptr};
    std::atomic<bool> main_task_running {false};
};

Controller* start_gprs_controller(esp_event_loop_handle_t event_loop)
{
    return new GprsController(event_loop);
}


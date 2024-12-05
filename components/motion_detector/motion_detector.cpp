#include <motion_detector.h>
#include <common.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

//to be configured via project menu config GPIO_PIR_1
constexpr auto GPIO_PIR_1 = static_cast<gpio_num_t>(0);

namespace {
QueueHandle_t gpio_evt_queue {nullptr};    
void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
}
struct MotionDetectorImpl : public MotionDetector
{
    MotionDetectorImpl()
    {
        gpio_set_direction(GPIO_PIR_1, GPIO_MODE_INPUT);
        gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    }
    bool run() override
    {
        //xTaskCreate(pir_monitor_task,"pir-monitor", 4096, main_event_loop,  5, NULL);
        return false;
    }
    void stop() override
    {

    }
    void release()
    {
        delete this;
    }
};

MotionDetector* MotionDetector::create_instance(esp_event_loop_handle_t event_handler,const char* type)
{
    return new MotionDetectorImpl();
}

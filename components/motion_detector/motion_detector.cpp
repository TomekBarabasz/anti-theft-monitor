#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <common.h>

#include <motion_detector.h>

namespace {
//to be configured via project menu config GPIO_PIR_1
constexpr auto GPIO_INPUT_PIR_1 = static_cast<gpio_num_t>(0);
constexpr uint64_t input_pin_mask = 1ULL << GPIO_INPUT_PIR_1;
static const char *TAG = "MOTD";
QueueHandle_t gpio_evt_queue {nullptr};

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    auto gpio_num = reinterpret_cast<uint32_t>(arg);
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
}
struct MotionDetectorImpl : public MotionDetector
{
    MotionDetectorImpl(esp_event_loop_handle_t el) : event_loop(el)
    {
        //gpio_set_direction(GPIO_PIR_1, GPIO_MODE_INPUT);
        gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
        gpio_config_t io_conf = {};
        //GPIO_INTR_POSEDGE
        //GPIO_INTR_ANYEDGE
        io_conf.intr_type = GPIO_INTR_NEGEDGE;  //interrupt of falling edge
        io_conf.pin_bit_mask = input_pin_mask;  //bit mask of the pins
        io_conf.mode = GPIO_MODE_INPUT; //set as input mode
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE; //enable pull-up mode
        gpio_config(&io_conf);

        //install gpio isr service
        gpio_install_isr_service(0);    //ESP_INTR_FLAG_DEFAULT
    }
    bool run() override
    {
        xTaskCreate(task_main, "motion_detector_task", 2048, this, 10, &task_handle);
        //hook isr handler for specific gpio pin
        gpio_isr_handler_add(GPIO_INPUT_PIR_1, gpio_isr_handler, (void*) GPIO_INPUT_PIR_1);
        return true;
    }
    void stop() override
    {
        vTaskDelete(task_handle);
        task_handle = nullptr;
        gpio_isr_handler_remove(GPIO_INPUT_PIR_1);
    }
    static void task_main(void *params)
    {
        auto & inst = *static_cast<MotionDetectorImpl*>(params);
        gpio_num_t io_num;
        for (;;) 
        {
            if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
            {
                auto lvl = gpio_get_level(io_num);
                //do some debouncing or validation with 2nd sensor
                void* event_data = nullptr;
                size_t event_data_size = 0;
                ESP_LOGI(TAG, "got event from gpio isr lvl=%d",lvl);
                ESP_ERROR_CHECK(esp_event_post_to(inst.event_loop, HARDWARE_BASED_EVENTS, evMotionDetected, event_data, event_data_size, portMAX_DELAY));
            }
        }
    }
    void release()
    {
        if (task_handle) {
            stop();
        }
        delete this;
    }
    TaskHandle_t task_handle {nullptr};
    esp_event_loop_handle_t event_loop {nullptr};
};

MotionDetector* MotionDetector::create_instance(esp_event_loop_handle_t event_loop,const char* type)
{
    return new MotionDetectorImpl(event_loop);
}

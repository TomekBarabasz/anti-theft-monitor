#include "driver/gpio.h"
#include "esp_log.h"
#include <spray_releaser.h>
#include <events.h>

constexpr auto MOTOR_GPIO = static_cast<gpio_num_t>(33); //INTERNAL RED LED
constexpr int MOTOR_ACTIVE_PERIOD_MS = 3000;
constexpr int MOTOR_GPIO_LVL_ACTIVE = 0;
constexpr int MOTOR_GPIO_LVL_INACTIVE = 1;

#ifdef ENABLE_SPR_TRACE
static const char* TAG = "SPR";
#define TRACE(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
BLEBLE
#else
#define TRACE(fmt, ...)
#endif

struct SprayReleaserImpl : public SprayReleaser
{    
    enum InternalCmd {
        cmdActivate = 1,
        cmdDeactivate = 2,
    };
    SprayReleaserImpl()
    {
        gpio_set_direction(MOTOR_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(MOTOR_GPIO, MOTOR_GPIO_LVL_INACTIVE);
    }
    bool run() override
    {
        evt_queue = xQueueCreate(3, sizeof(uint32_t));
        xTaskCreate(task_main, "spray_releaser_task", 2048, this, 10, &task_handle);
        return true;
    }
    virtual bool activate() override
    {
        uint32_t cmd = cmdActivate;
        TRACE("Activating");
        return xQueueSendToFront(evt_queue,&cmd,portMAX_DELAY) == pdPASS;
    }
    virtual bool deactivate() override
    {
        uint32_t cmd = cmdDeactivate;
        return xQueueSendToFront(evt_queue,&cmd,portMAX_DELAY) == pdPASS;
    }
    void stop() override
    {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
    static void task_main(void *params)
    {
        auto & inst = *static_cast<SprayReleaserImpl*>(params);
        for (;;) 
        {
            uint32_t cmd;
            if (xQueueReceive(inst.evt_queue, &cmd, portMAX_DELAY)) 
            {
                if (cmdActivate == cmd) 
                {
                    // rework to allow cancelling
                    TRACE("Set motor gpio ACTIVE");
                    gpio_set_level(MOTOR_GPIO, MOTOR_GPIO_LVL_ACTIVE);
                    vTaskDelay(MOTOR_ACTIVE_PERIOD_MS / portTICK_PERIOD_MS);
                    TRACE("Set motor gpio INACTIVE");
                    gpio_set_level(MOTOR_GPIO, MOTOR_GPIO_LVL_INACTIVE);
                }
            }
        }
        vTaskDelete(nullptr);
    }
    QueueHandle_t evt_queue {nullptr};
    TaskHandle_t task_handle {nullptr};
};

SprayReleaser* SprayReleaser::create_instance()
{
    return new SprayReleaserImpl();
}

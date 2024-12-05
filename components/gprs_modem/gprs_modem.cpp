#include <gprs_modem.h>
#include <common.h>

struct GprsModemImpl : public GprsModem
{
    GprsModemImpl()
    {
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

GprsModem* GprsModem::create_instance(esp_event_loop_handle_t event_handler,const char* type)
{
    return new GprsModemImpl();
}

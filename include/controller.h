#include "esp_event.h"

struct Controller
{
    static Controller* create_instance(esp_event_loop_handle_t event_loop,const char* type, void *params = nullptr);

    virtual bool run() = 0;
    virtual void stop() = 0;
    virtual void release() = 0;
protected:
    virtual ~Controller(){}
};
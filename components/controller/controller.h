#include "esp_event.h"
#include <commands.h>

struct Controller
{
    static void delete_instance(Controller*);
    virtual bool start() = 0;
    virtual void stop() = 0;
protected:
    virtual ~Controller(){}
};

Controller* create_tcp_controller(esp_event_loop_handle_t event_loop, const CmdStartTcpController&);
Controller* create_gprs_controller(esp_event_loop_handle_t event_loop);

#include "esp_event.h"

struct MotionDetector
{
    static MotionDetector* create_instance(esp_event_loop_handle_t event_handler,const char* type = nullptr);

    virtual bool run()  = 0;
    virtual void stop() = 0;
    virtual void release() = 0;
protected:
    virtual ~MotionDetector(){}
};

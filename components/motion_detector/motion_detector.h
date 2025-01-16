#include "esp_event.h"

struct MotionDetector
{
    static MotionDetector* create_instance(esp_event_loop_handle_t event_handler);
    static void delete_instance(MotionDetector*);
    
    virtual bool start()  = 0;
    virtual void stop() = 0;

protected:
    virtual ~MotionDetector(){}
};

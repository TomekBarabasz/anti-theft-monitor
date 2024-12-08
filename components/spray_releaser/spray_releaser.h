#include "esp_event.h"

struct SprayReleaser
{
    static SprayReleaser* create_instance();

    virtual bool run()  = 0;
    virtual void stop() = 0;
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual void release() = 0;
protected:
    virtual ~SprayReleaser(){}
};

#include "esp_event.h"

struct SprayReleaser
{
    static SprayReleaser* create_instance();
    static void delete_instance(SprayReleaser*);

    virtual bool run()  = 0;
    virtual void stop() = 0;
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;

protected:
    virtual ~SprayReleaser(){}
};

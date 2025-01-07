#include <string>
#include <fmt/core.h>

struct EventMonitor
{
    enum class Severity {
        debug,
        info,
        error
    };
    static EventMonitor* create_instance(const char* type,void* prms = nullptr);
    static EventMonitor* get_instance();

    virtual bool send(EventMonitor::Severity s,const char* tag, std::string&& event) = 0;
    virtual void release() = 0;
    
protected:
    virtual ~EventMonitor() {}
};

#define _LOG_E_(TAG,fmts,...) EventMonitor::get_instance()->send(\
    EventMonitor::Severity::error, TAG, fmt::format(fmts, ##__VA_ARGS__));
#define _LOG_I_(TAG,fmts,...) EventMonitor::get_instance()->send(\
    EventMonitor::Severity::info, TAG, fmt::format(fmts, ##__VA_ARGS__));

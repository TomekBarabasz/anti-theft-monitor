#include <string>

struct EventMonitor
{
    enum class Severity {
        debug,
        info,
        error
    };
    static EventMonitor* create_instance(const char* type,void* prms);
    static EventMonitor* get_instance();

    virtual bool send(EventMonitor::Severity s,std::string&& event) = 0;
    virtual void release() = 0;
    
protected:
    virtual ~EventMonitor() {}
};

#include <string>

struct EventMonitor
{
    static EventMonitor* create_instance(const char* type);
    static EventMonitor* get_instance();

    virtual bool send(std::string&& event) = 0;
    virtual void release() = 0;
    
protected:
    virtual ~EventMonitor() {}
};

#include <commands.h>



struct EventMonitor
{
    static void create_udp(const CmdStartUdpMonitor&);
    static void create_serial();

    static EventMonitor* get_instance() { return p_instance;}
    static void delete_instance(EventMonitor*);
    virtual int start() = 0;
    virtual int send(const char*data,int size) = 0;
    virtual void stop() = 0;

protected:
    virtual ~EventMonitor(){}
    inline static EventMonitor* p_instance {nullptr};
};

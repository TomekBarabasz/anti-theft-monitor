#include <event_monitor.h>
#include <cstdint>
#include <lwip/sockets.h>

namespace {
struct UdpMonitor : public EventMonitor
{
    sockaddr_in dest_addr;
    int sockfd {-1};
    UdpMonitor(const CmdStartUdpMonitor& ipp)
    {
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(ipp.port);
        dest_addr.sin_addr.s_addr = ((uint32_t)ipp.ip[0] << 24) 
                                | ((uint32_t)ipp.ip[1] << 16) 
                                | ((uint32_t)ipp.ip[2] << 8) 
                                |  (uint32_t)ipp.ip[3];
    }
    int start() override
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            return -errno;
        }
        return sockfd;
    }
    int send(const char* data, int len) override
    {
        return sendto(sockfd, data, len, 0, 
                    (const struct sockaddr *)&dest_addr, sizeof(dest_addr));
    }
    void stop() override
    {
        ::close(sockfd);
        sockfd = -1;
        memset(&dest_addr, 0, sizeof(dest_addr));
    }
};
}

void EventMonitor::create_udp(const CmdStartUdpMonitor& cmd)
{
    if (p_instance) {
        delete p_instance;
    }
    p_instance = new UdpMonitor(cmd);
}

#pragma once
#include <cstdint>

enum class WifiMode : uint8_t {
    AP,
    STA
};

struct CmdStartUdpMonitor {
    uint8_t ip[4];
    uint16_t port;
};

struct CmdStartTcpController {
    uint16_t port;
    WifiMode wifi_mode;
    char ssid[32];
    char password[64];
};

struct CmdEcho {
    uint16_t n_chars;
    char message[1];
};

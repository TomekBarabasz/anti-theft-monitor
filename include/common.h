#include "esp_event.h"

ESP_EVENT_DEFINE_BASE(EXTERNAL_COMMAND_EVENTS);
enum ServerCommandEvents
{
    evAuthenticate,
    evDisarmSprayReleaser,
    evArmSprayReleaser,
    evFlashOn,
    evFlashOff,
    evFlashBlink,
    evStartVoiceCall,
    evEndVoiceCall,
    evCapturePhoto,
    evStartAudioStreaming,
    evStartVideoStreaming,
    evPairBluetooth,
    evStartTcpController,
    evStopTcpController,
    evStartBluetooth,
    evStartUpdMonitor,
    evStopUpdMonitor,
    evEcho,
    evStartGprsController,
    
    evTcpControllerStopped,
    evGprsControllerStopped
};

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

ESP_EVENT_DEFINE_BASE(HARDWARE_BASED_EVENTS);
enum HardwareBasedEvents : uint8_t 
{
    evMotionDetected,
    evBtnPressed_1,
    evBtnReleased_1
};

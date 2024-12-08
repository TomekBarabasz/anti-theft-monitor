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
    evStartStreaming,
    evPairBluetooth,
    evStartTcpController,
    evStartBluetooth
};

struct evStartTcpControllerParams
{
    int port;
    enum WifiMode : uint8_t {
        AP,
        STA
    } wifi_mode;
    char ssid[64];
    char password[64];
};

ESP_EVENT_DEFINE_BASE(HARDWARE_BASED_EVENTS);
enum HardwareBasedEvents 
{
    evMotionDetected
};


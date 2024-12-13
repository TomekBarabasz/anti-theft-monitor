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
    evTcpControllerStopped,
    evStartBluetooth
};

enum class WifiMode : uint8_t {
    AP,
    STA
};

struct evStartTcpControllerParams
{
    int port;
    WifiMode wifi_mode;
    char ssid[32];
    char password[64];
};

ESP_EVENT_DEFINE_BASE(HARDWARE_BASED_EVENTS);
enum HardwareBasedEvents 
{
    evMotionDetected,
    evBtnPressed_1,
    evBtnReleased_1
};


#pragma once
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

ESP_EVENT_DEFINE_BASE(HARDWARE_BASED_EVENTS);
enum HardwareBasedEvents : uint8_t 
{
    evMotionDetected,
    evBtnPressed_1,
    evBtnReleased_1
};

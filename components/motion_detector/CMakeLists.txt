idf_component_register(
SRCS
    motion_detector.cpp
INCLUDE_DIRS
    .
    ${CMAKE_SOURCE_DIR}/include
REQUIRES
    esp_event
    driver
    event_monitor
)

idf_component_register(
SRCS
    spray_releaser.cpp

INCLUDE_DIRS
    .
    ${CMAKE_SOURCE_DIR}/include

REQUIRES
    esp_event
    driver
)

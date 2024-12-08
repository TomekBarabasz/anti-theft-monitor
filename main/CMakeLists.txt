idf_component_register(
SRCS 
    main.cpp

INCLUDE_DIRS 
    .
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/components/motion_detector
    ${CMAKE_SOURCE_DIR}/components/controller
    ${CMAKE_SOURCE_DIR}/components/spray_releaser

REQUIRES
    esp_netif
    nvs_flash
)

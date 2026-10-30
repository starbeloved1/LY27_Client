find_path(PAHO_INCLUDE_DIR MQTTAsync.h REQUIRED)
find_library(PAHO_ASYNC_LIBRARY NAMES paho-mqtt3a REQUIRED)

add_library(ly27_mqtt STATIC
    include/MqttTransport.hpp
    src/MqttTransport.cpp
)
target_include_directories(ly27_mqtt
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${PAHO_INCLUDE_DIR}"
)
target_link_libraries(ly27_mqtt PUBLIC ly27_config PRIVATE "${PAHO_ASYNC_LIBRARY}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

add_library(mqtt STATIC
    MqttClient.cpp
    MqttClient.h
)

target_link_libraries(mqtt
    PRIVATE
        PkgConfig::MOSQUITTO
)

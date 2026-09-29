find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

add_library(mqtt STATIC
    MqttClient.cpp
    MqttClient.h
    TelemetryAdapter.cpp
    TelemetryAdapter.h
)

target_include_directories(mqtt
    PUBLIC
        ${CMAKE_SOURCE_DIR}/backend
)

target_link_libraries(mqtt
    PUBLIC
        application
        protocol

    PRIVATE
        PkgConfig::MOSQUITTO
)

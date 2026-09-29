add_library(domain INTERFACE)

target_sources(domain
    INTERFACE
        DeviceId.h
        Measurement.h
)

target_include_directories(domain
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

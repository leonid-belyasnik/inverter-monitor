add_library(domain INTERFACE)

target_sources(domain
    INTERFACE
        Measurement.h
)

target_include_directories(domain
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

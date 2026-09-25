add_library(protocol STATIC
    TelemetryParser.cpp
    TelemetryParser.h
)

target_include_directories(protocol
    PUBLIC
        ${CMAKE_SOURCE_DIR}/backend
)

target_link_libraries(protocol
    PUBLIC
        domain
        Boost::json
)

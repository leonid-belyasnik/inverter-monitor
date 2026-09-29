add_library(application STATIC
    TelemetryIngestionService.cpp
    IngestedMeasurement.h
    TelemetryIngestionService.h
)

target_include_directories(application
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(application
    PUBLIC
        domain

    PRIVATE
        database
)

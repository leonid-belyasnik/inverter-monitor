add_library(application STATIC
    IngestedMeasurement.h
    MeasurementQueryService.cpp
    TelemetryIngestionService.cpp
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

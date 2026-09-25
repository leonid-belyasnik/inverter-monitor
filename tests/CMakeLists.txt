find_package(GTest REQUIRED)

add_executable(measurement_repository_test
    MeasurementRepositoryTest.cpp
)

target_link_libraries(measurement_repository_test
    PRIVATE
        database
        GTest::gtest_main
)

add_test(
    NAME MeasurementRepositoryTest
    COMMAND measurement_repository_test
)

add_executable(telemetry_parser_test
    TelemetryParserTest.cpp
)

target_link_libraries(telemetry_parser_test
    PRIVATE
        protocol
        GTest::gtest_main
)

add_test(
    NAME TelemetryParserTest
    COMMAND telemetry_parser_test
)

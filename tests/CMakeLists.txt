find_package(GTest REQUIRED)

add_executable(measurement_repository_test
    MeasurementRepositoryTest.cpp
)

target_link_libraries(measurement_repository_test
    PRIVATE
        application
        database
        mqtt
        GTest::gtest_main
)

add_test(
    NAME MeasurementRepositoryTest
    COMMAND measurement_repository_test
)

add_executable(migration_runner_test
    MigrationRunnerTest.cpp
)

target_link_libraries(migration_runner_test
    PRIVATE
        database
        GTest::gtest_main
)

add_test(
    NAME MigrationRunnerTest
    COMMAND migration_runner_test
)

add_executable(postgres_diagnostics_test
    PostgresDiagnosticsTest.cpp
)

target_link_libraries(postgres_diagnostics_test
    PRIVATE
        application
        database
        GTest::gtest_main
)

add_test(
    NAME PostgresDiagnosticsTest
    COMMAND postgres_diagnostics_test
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

add_executable(telemetry_adapter_test
    TelemetryAdapterTest.cpp
)

target_link_libraries(telemetry_adapter_test
    PRIVATE
        mqtt
        GTest::gtest_main
)

add_test(
    NAME TelemetryAdapterTest
    COMMAND telemetry_adapter_test
)

add_executable(http_router_test
    HttpRouterTest.cpp
)

target_link_libraries(http_router_test
    PRIVATE
        api
        GTest::gtest_main
)

add_test(
    NAME HttpRouterTest
    COMMAND http_router_test
)

add_executable(api_routes_test
    ApiRoutesTest.cpp
)

target_link_libraries(api_routes_test
    PRIVATE
        api
        application
        database
        GTest::gtest_main
)

add_test(
    NAME ApiRoutesTest
    COMMAND api_routes_test
)

add_executable(http_server_test
    HttpServerTest.cpp
)

target_link_libraries(http_server_test
    PRIVATE
        api
        GTest::gtest_main
)

add_test(
    NAME HttpServerTest
    COMMAND http_server_test
)

add_executable(websocket_client
    WebSocketClient.cpp
)

target_link_libraries(websocket_client
    PRIVATE
        Boost::headers
)

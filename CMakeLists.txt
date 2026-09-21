cmake_minimum_required(VERSION 3.20)

project(inverter-monitor
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQ REQUIRED IMPORTED_TARGET libpq)

set(sources
    src/main.cpp
    src/PostgresConnection.cpp
    src/PostgresResult.cpp
)

set(headers
    src/PostgresConnection.h
    src/PostgresResult.h
)

add_executable(inverter-monitor)

target_sources(inverter-monitor
    PRIVATE
        ${sources}
        ${headers}
)

target_link_libraries(inverter-monitor
    PRIVATE
        PkgConfig::LIBPQ
)

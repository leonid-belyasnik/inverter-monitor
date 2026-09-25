cmake_minimum_required(VERSION 3.20)

project(inverter-monitor
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.75 REQUIRED COMPONENTS json)

add_subdirectory(domain)
add_subdirectory(database)
add_subdirectory(backend)
add_subdirectory(simulator)


include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

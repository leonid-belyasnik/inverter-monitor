find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

add_executable(gateway-simulator)

target_sources(gateway-simulator
    PRIVATE
        main.cpp
)

target_link_libraries(gateway-simulator
    PRIVATE
    	domain
        PkgConfig::MOSQUITTO
)

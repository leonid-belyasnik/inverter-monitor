add_subdirectory(api)
add_subdirectory(application)
add_subdirectory(mqtt)
add_subdirectory(protocol)

add_executable(inverter-monitor)

target_sources(inverter-monitor
    PRIVATE
        main.cpp
)

target_link_libraries(inverter-monitor
    PRIVATE
        application
        database
        mqtt
        protocol
)

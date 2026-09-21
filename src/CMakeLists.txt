add_executable(inverter-monitor)

target_sources(inverter-monitor
    PRIVATE
        main.cpp
)

target_link_libraries(inverter-monitor
    PRIVATE
        database
)

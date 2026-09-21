add_executable(measurement_repository_test)

target_sources(measurement_repository_test
    PRIVATE
        MeasurementRepositoryTest.cpp
)

target_link_libraries(measurement_repository_test
    PRIVATE
        database
)

add_test(
    NAME MeasurementRepositoryTest
    COMMAND measurement_repository_test
)

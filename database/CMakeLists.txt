find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQ REQUIRED IMPORTED_TARGET libpq)

add_library(database STATIC)

set(sources
    DatabaseSchema.cpp
    MeasurementRepository.cpp
    MigrationRunner.cpp
    Migrations.cpp
    PostgresConnection.cpp
    PostgresResult.cpp
)

set(headers
    DatabaseSchema.h
    MeasurementRepository.h
    Migration.h
    MigrationRunner.h
    Migrations.h
    PostgresConnection.h
    PostgresResult.h
)

target_sources(database
    PRIVATE
        ${sources}
        ${headers}
)

target_include_directories(database
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(database
    PRIVATE
        PkgConfig::LIBPQ

    PUBLIC
        domain
)

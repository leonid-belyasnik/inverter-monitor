add_library(api STATIC
    ApiRoutes.cpp
    HttpRouter.cpp
    HttpServer.cpp
)

target_include_directories(api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(api
    PUBLIC
        application
        Boost::headers
        Boost::json
)

add_executable(api-server
    ApiServerMain.cpp
)

target_link_libraries(api-server
    PRIVATE
        api
)

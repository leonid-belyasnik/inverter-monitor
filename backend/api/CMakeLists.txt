add_library(api STATIC
    ApiFormatting.cpp
    ApiRoutes.cpp
    HttpRouter.cpp
    HttpServer.cpp
    WebSocketServer.cpp
    WebSocketSession.cpp
    WebSocketProtocol.cpp
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

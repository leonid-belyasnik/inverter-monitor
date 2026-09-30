#include "HttpServer.h"

int main()
{
    api::HttpServer server("0.0.0.0", 8080);
    server.run();
}

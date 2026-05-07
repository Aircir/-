#pragma once

#include "http_types.h"

#include <functional>
#include <string>

class HttpServer {
public:
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

    HttpServer(std::string host, unsigned short port);
    ~HttpServer();

    bool start(std::string& error_message);
    void serve(const RequestHandler& handler);
    void stop();

private:
    std::string host_;
    unsigned short port_;
    bool running_ = false;
#ifdef _WIN32
    unsigned long long listen_socket_ = static_cast<unsigned long long>(~0ULL);
#else
    int listen_socket_ = -1;
#endif
};

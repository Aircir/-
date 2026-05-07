#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string target;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> form_params;
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string content_type = "application/json; charset=utf-8";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

#include "http_server.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

constexpr SOCKET kInvalidSocket = INVALID_SOCKET;

std::string to_lower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string url_decode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];

        if (ch == '+') {
            decoded.push_back(' ');
            continue;
        }

        if (ch == '%' && index + 2 < value.size()) {
            const std::string hex = value.substr(index + 1, 2);
            char* end_ptr = nullptr;
            const long code = std::strtol(hex.c_str(), &end_ptr, 16);
            if (end_ptr != nullptr && *end_ptr == '\0') {
                decoded.push_back(static_cast<char>(code));
                index += 2;
                continue;
            }
        }

        decoded.push_back(ch);
    }

    return decoded;
}

std::unordered_map<std::string, std::string> parse_query_params(const std::string& query_string) {
    std::unordered_map<std::string, std::string> params;
    std::size_t start = 0;

    while (start <= query_string.size()) {
        const std::size_t end = query_string.find('&', start);
        const std::string pair = query_string.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::size_t separator = pair.find('=');

        if (!pair.empty()) {
            if (separator == std::string::npos) {
                params[url_decode(pair)] = "";
            } else {
                params[url_decode(pair.substr(0, separator))] = url_decode(pair.substr(separator + 1));
            }
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    return params;
}

std::optional<std::size_t> expected_request_size(const std::string& request_text) {
    const std::size_t header_end = request_text.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return std::nullopt;
    }

    std::istringstream stream(request_text.substr(0, header_end));
    std::string line;
    std::size_t content_length = 0;

    if (!std::getline(stream, line)) {
        return std::nullopt;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = to_lower(trim_copy(line.substr(0, separator)));
        if (key != "content-length") {
            continue;
        }

        try {
            content_length = static_cast<std::size_t>(std::stoull(trim_copy(line.substr(separator + 1))));
        } catch (...) {
            content_length = 0;
        }
        break;
    }

    return header_end + 4 + content_length;
}

bool parse_request(const std::string& request_text, HttpRequest& request) {
    const std::size_t header_end = request_text.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return false;
    }

    const std::string header_block = request_text.substr(0, header_end);
    request.body = request_text.substr(header_end + 4);
    request.headers.clear();
    request.form_params.clear();

    std::istringstream stream(header_block);
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        return false;
    }

    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream line_stream(request_line);
    std::string method;
    std::string target;
    std::string http_version;
    line_stream >> method >> target >> http_version;

    if (method.empty() || target.empty() || http_version.empty()) {
        return false;
    }

    request.method = method;
    request.target = target;

    std::string header_line;
    while (std::getline(stream, header_line)) {
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }

        const std::size_t separator = header_line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = to_lower(trim_copy(header_line.substr(0, separator)));
        const std::string value = trim_copy(header_line.substr(separator + 1));
        request.headers[key] = value;
    }

    const std::size_t query_separator = target.find('?');
    if (query_separator == std::string::npos) {
        request.path = target;
        request.query_params.clear();
    } else {
        request.path = target.substr(0, query_separator);
        request.query_params = parse_query_params(target.substr(query_separator + 1));
    }

    const auto content_type = request.headers.find("content-type");
    if (content_type != request.headers.end()) {
        const std::string normalized_content_type = to_lower(content_type->second);
        if (normalized_content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
            request.form_params = parse_query_params(request.body);
        }
    }

    return true;
}

std::string build_http_response_head(const HttpResponse& response) {
    std::ostringstream output;
    output << "HTTP/1.1 " << response.status_code << ' ' << response.status_text << "\r\n";
    output << "Content-Type: " << response.content_type << "\r\n";
    if (!response.streaming) {
        output << "Content-Length: " << response.body.size() << "\r\n";
    }
    output << "Connection: close\r\n";
    output << "Access-Control-Allow-Origin: *\r\n";
    output << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    output << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";

    for (const auto& [key, value] : response.headers) {
        output << key << ": " << value << "\r\n";
    }

    output << "\r\n";
    return output.str();
}

std::string build_http_response(const HttpResponse& response) {
    std::ostringstream output;
    output << build_http_response_head(response);
    output << response.body;
    return output.str();
}

bool send_all(SOCKET client_socket, const std::string& payload) {
    std::size_t total_sent = 0;

    while (total_sent < payload.size()) {
        const int sent = send(client_socket, payload.data() + total_sent, static_cast<int>(payload.size() - total_sent), 0);
        if (sent == SOCKET_ERROR) {
            return false;
        }
        total_sent += static_cast<std::size_t>(sent);
    }

    return true;
}

}  // namespace

HttpServer::HttpServer(std::string host, unsigned short port)
    : host_(std::move(host)), port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(std::string& error_message) {
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        error_message = "WSAStartup failed.";
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* result = nullptr;
    const std::string port_text = std::to_string(port_);
    const int getaddrinfo_status = getaddrinfo(host_.c_str(), port_text.c_str(), &hints, &result);
    if (getaddrinfo_status != 0) {
        error_message = "getaddrinfo failed.";
        WSACleanup();
        return false;
    }

    listen_socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket_ == kInvalidSocket) {
        error_message = "socket creation failed.";
        freeaddrinfo(result);
        WSACleanup();
        return false;
    }

    BOOL reuse_address = TRUE;
    setsockopt(static_cast<SOCKET>(listen_socket_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse_address), sizeof(reuse_address));

    if (bind(static_cast<SOCKET>(listen_socket_), result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        error_message = "bind failed. Check whether the port is already in use.";
        freeaddrinfo(result);
        closesocket(static_cast<SOCKET>(listen_socket_));
        listen_socket_ = kInvalidSocket;
        WSACleanup();
        return false;
    }

    freeaddrinfo(result);

    if (listen(static_cast<SOCKET>(listen_socket_), SOMAXCONN) == SOCKET_ERROR) {
        error_message = "listen failed.";
        closesocket(static_cast<SOCKET>(listen_socket_));
        listen_socket_ = kInvalidSocket;
        WSACleanup();
        return false;
    }

    running_ = true;
    return true;
}

void HttpServer::serve(const RequestHandler& handler) {
    if (!running_) {
        return;
    }

    std::cout << "Backend server is listening on http://" << host_ << ":" << port_ << std::endl;

    while (running_) {
        SOCKET client_socket = accept(static_cast<SOCKET>(listen_socket_), nullptr, nullptr);
        if (client_socket == kInvalidSocket) {
            continue;
        }

        std::string request_text;
        std::array<char, 4096> buffer{};

        while (true) {
            const int received = recv(client_socket, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received <= 0) {
                break;
            }

            request_text.append(buffer.data(), received);

            const auto expected_size = expected_request_size(request_text);
            if (expected_size.has_value() && request_text.size() >= expected_size.value()) {
                break;
            }
        }

        HttpResponse response;
        HttpRequest request;

        if (!parse_request(request_text, request)) {
            response.status_code = 400;
            response.status_text = "Bad Request";
            response.body = "{\"code\":400,\"message\":\"Invalid HTTP request.\",\"data\":null}";
        } else {
            response = handler(request);
        }

        if (response.streaming && response.stream_handler) {
            const std::string header_payload = build_http_response_head(response);
            if (send_all(client_socket, header_payload)) {
                const HttpStreamWriter writer = [&](const std::string& payload) {
                    return send_all(client_socket, payload);
                };
                response.stream_handler(writer);
            }
        } else {
            const std::string http_payload = build_http_response(response);
            send_all(client_socket, http_payload);
        }
        shutdown(client_socket, SD_BOTH);
        closesocket(client_socket);
    }
}

void HttpServer::stop() {
    if (listen_socket_ != kInvalidSocket) {
        closesocket(static_cast<SOCKET>(listen_socket_));
        listen_socket_ = kInvalidSocket;
    }

    if (running_) {
        running_ = false;
    }

    WSACleanup();
}

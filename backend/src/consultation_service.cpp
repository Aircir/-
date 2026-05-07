#include "consultation_service.h"
#include "json_utils.h"
#include "persistence_utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

struct ParsedUrl {
    bool secure = true;
    std::wstring host;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    std::wstring path;
};

struct HttpResult {
    int status_code = 0;
    std::string body;
    std::string error_message;
};

std::string narrow_ascii(const std::wstring& value) {
    std::string result;
    result.reserve(value.size());
    for (wchar_t ch : value) {
        result.push_back(ch <= 127 ? static_cast<char>(ch) : '?');
    }
    return result;
}

std::string format_winhttp_error(const std::string& stage, DWORD error_code) {
    LPWSTR raw_message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, error_code, 0, reinterpret_cast<LPWSTR>(&raw_message), 0, nullptr);

    std::ostringstream output;
    output << stage << " failed (WinHTTP error " << error_code << ")";

    if (length > 0 && raw_message != nullptr) {
        std::wstring message(raw_message, length);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }
        if (!message.empty()) {
            output << ": " << narrow_ascii(message);
        }
    }

    if (raw_message != nullptr) {
        LocalFree(raw_message);
    }

    return output.str();
}

std::string escape_json_text(const std::string& value) {
    std::ostringstream output;

    for (unsigned char ch : value) {
        switch (ch) {
            case '\\':
                output << "\\\\";
                break;
            case '"':
                output << "\\\"";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    output << "\\u00";
                    static const char* hex = "0123456789abcdef";
                    output << hex[(ch >> 4U) & 0x0FU] << hex[ch & 0x0FU];
                } else {
                    output << static_cast<char>(ch);
                }
                break;
        }
    }

    return output.str();
}

std::string quote_json_text(const std::string& value) {
    return "\"" + escape_json_text(value) + "\"";
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::ostringstream output;

    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            output << "\n";
        }
        output << lines[index];
    }

    return output.str();
}

std::string trim_copy_local(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::uint64_t parse_u64_or_zero(const std::string& value) {
    try {
        return value.empty() ? 0 : static_cast<std::uint64_t>(std::stoull(value));
    } catch (...) {
        return 0;
    }
}

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string normalize_base_url(std::string base_url) {
    base_url = trim_copy_local(std::move(base_url));

    while (!base_url.empty() && base_url.back() == '/') {
        base_url.pop_back();
    }

    if (ends_with(base_url, "/responses")) {
        base_url.resize(base_url.size() - 10);
    } else if (ends_with(base_url, "/chat/completions")) {
        base_url.resize(base_url.size() - 17);
    }

    while (!base_url.empty() && base_url.back() == '/') {
        base_url.pop_back();
    }

    return base_url;
}

std::optional<std::string> extract_json_string_by_key(const std::string& json, const std::string& key, std::size_t start_pos = 0) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = json.find(token, start_pos);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colon_pos = json.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t value_pos = colon_pos + 1;
    while (value_pos < json.size() && std::isspace(static_cast<unsigned char>(json[value_pos]))) {
        ++value_pos;
    }

    if (value_pos >= json.size() || json[value_pos] != '"') {
        return std::nullopt;
    }

    ++value_pos;
    std::string result;

    while (value_pos < json.size()) {
        const char ch = json[value_pos++];
        if (ch == '\\') {
            if (value_pos >= json.size()) {
                break;
            }

            const char escaped = json[value_pos++];
            switch (escaped) {
                case '"':
                    result.push_back('"');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                case '/':
                    result.push_back('/');
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u':
                    if (value_pos + 4 <= json.size()) {
                        value_pos += 4;
                        result.push_back('?');
                    }
                    break;
                default:
                    result.push_back(escaped);
                    break;
            }
            continue;
        }

        if (ch == '"') {
            return result;
        }

        result.push_back(ch);
    }

    return std::nullopt;
}

std::optional<std::string> extract_responses_output_text(const std::string& json) {
    std::size_t search_pos = 0;

    while (search_pos < json.size()) {
        const std::size_t type_pos = json.find("\"type\":\"output_text\"", search_pos);
        if (type_pos == std::string::npos) {
            break;
        }

        const auto text = extract_json_string_by_key(json, "text", type_pos);
        if (text.has_value()) {
            return text;
        }

        search_pos = type_pos + 1;
    }

    const auto top_level_text = extract_json_string_by_key(json, "output_text");
    if (top_level_text.has_value()) {
        return top_level_text;
    }

    return std::nullopt;
}

std::optional<std::string> extract_chat_output_text(const std::string& json) {
    const std::size_t choices_pos = json.find("\"choices\"");
    if (choices_pos == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t message_pos = json.find("\"message\"", choices_pos);
    if (message_pos == std::string::npos) {
        return std::nullopt;
    }

    const auto content = extract_json_string_by_key(json, "content", message_pos);
    if (content.has_value()) {
        return content;
    }

    return extract_json_string_by_key(json, "text", message_pos);
}

std::string extract_error_message(const std::string& json) {
    const auto message = extract_json_string_by_key(json, "message");
    return message.has_value() ? message.value() : std::string(u8"AI 接口请求失败。");
}

bool should_try_chat_fallback(const HttpResult& result) {
    if (!result.error_message.empty()) {
        return false;
    }

    if (result.status_code == 404 || result.status_code == 405 || result.status_code == 415) {
        return true;
    }

    if (result.status_code != 400) {
        return false;
    }

    const std::string lowered = to_lower_copy(result.body);
    return lowered.find("unknown parameter") != std::string::npos
        || lowered.find("unsupported") != std::string::npos
        || lowered.find("invalid value") != std::string::npos
        || lowered.find("supported values are") != std::string::npos
        || lowered.find("input_text") != std::string::npos
        || lowered.find("output_text") != std::string::npos
        || lowered.find("refusal") != std::string::npos
        || lowered.find("not found") != std::string::npos
        || lowered.find("invalid url") != std::string::npos
        || lowered.find("unrecognized request argument") != std::string::npos
        || lowered.find("responses") != std::string::npos;
}

std::wstring utf8_to_wstring(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return L"";
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), required);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

ParsedUrl parse_url(const std::string& url_text) {
    ParsedUrl parsed;
    const std::wstring url = utf8_to_wstring(url_text);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components)) {
        return parsed;
    }

    parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.port = components.nPort;
    parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0 && components.lpszExtraInfo != nullptr) {
        parsed.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (parsed.path.empty()) {
        parsed.path = L"/";
    }
    return parsed;
}

HttpResult send_json_request(
    const std::string& url_text,
    const std::string& api_key,
    const std::string& body,
    int timeout_seconds
) {
    HttpResult result;
    const ParsedUrl parsed = parse_url(url_text);
    if (parsed.host.empty()) {
        result.error_message = u8"AI 接口地址解析失败。";
        return result;
    }

    HINTERNET session = WinHttpOpen(L"competition-system/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        result.error_message = format_winhttp_error("WinHttpOpen", GetLastError());
        return result;
    }

    const int timeout_ms = std::max(1, timeout_seconds) * 1000;
    WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    HINTERNET connection = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (connection == nullptr) {
        result.error_message = format_winhttp_error("WinHttpConnect", GetLastError());
        WinHttpCloseHandle(session);
        return result;
    }

    const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", parsed.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (request == nullptr) {
        result.error_message = format_winhttp_error("WinHttpOpenRequest", GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + utf8_to_wstring(api_key) + L"\r\n";
    const BOOL send_ok = WinHttpSendRequest(
        request,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0
    );

    if (!send_ok) {
        result.error_message = format_winhttp_error("WinHttpSendRequest", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        result.error_message = format_winhttp_error("WinHttpReceiveResponse", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
    result.status_code = static_cast<int>(status_code);

    std::string response_body;
    while (true) {
        DWORD available_size = 0;
        if (!WinHttpQueryDataAvailable(request, &available_size)) {
            result.error_message = format_winhttp_error("WinHttpQueryDataAvailable", GetLastError());
            break;
        }

        if (available_size == 0) {
            break;
        }

        std::string chunk(static_cast<std::size_t>(available_size), '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(request, chunk.data(), available_size, &downloaded)) {
            result.error_message = format_winhttp_error("WinHttpReadData", GetLastError());
            break;
        }

        chunk.resize(static_cast<std::size_t>(downloaded));
        response_body += chunk;
    }

    result.body = response_body;

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}

HttpResult send_json_request_stream(
    const std::string& url_text,
    const std::string& api_key,
    const std::string& body,
    int timeout_seconds,
    const std::function<bool(const std::string&)>& on_chunk
) {
    HttpResult result;
    const ParsedUrl parsed = parse_url(url_text);
    if (parsed.host.empty()) {
        result.error_message = u8"AI 接口地址解析失败。";
        return result;
    }

    HINTERNET session = WinHttpOpen(L"competition-system/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        result.error_message = format_winhttp_error("WinHttpOpen", GetLastError());
        return result;
    }

    const int timeout_ms = std::max(1, timeout_seconds) * 1000;
    WinHttpSetTimeouts(session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    HINTERNET connection = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (connection == nullptr) {
        result.error_message = format_winhttp_error("WinHttpConnect", GetLastError());
        WinHttpCloseHandle(session);
        return result;
    }

    const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", parsed.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (request == nullptr) {
        result.error_message = format_winhttp_error("WinHttpOpenRequest", GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + utf8_to_wstring(api_key) + L"\r\nAccept: text/event-stream\r\n";
    const BOOL send_ok = WinHttpSendRequest(
        request,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0
    );

    if (!send_ok) {
        result.error_message = format_winhttp_error("WinHttpSendRequest", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        result.error_message = format_winhttp_error("WinHttpReceiveResponse", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
    result.status_code = static_cast<int>(status_code);

    std::string response_body;
    while (true) {
        DWORD available_size = 0;
        if (!WinHttpQueryDataAvailable(request, &available_size)) {
            result.error_message = format_winhttp_error("WinHttpQueryDataAvailable", GetLastError());
            break;
        }

        if (available_size == 0) {
            break;
        }

        std::string chunk(static_cast<std::size_t>(available_size), '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(request, chunk.data(), available_size, &downloaded)) {
            result.error_message = format_winhttp_error("WinHttpReadData", GetLastError());
            break;
        }

        chunk.resize(static_cast<std::size_t>(downloaded));
        response_body += chunk;
        if (on_chunk && !on_chunk(chunk)) {
            result.error_message = u8"流式传输已中断。";
            break;
        }
    }

    result.body = response_body;

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}

std::optional<std::uint64_t> extract_json_u64_by_key(const std::string& json, const std::string& key, std::size_t start_pos = 0) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = json.find(token, start_pos);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colon_pos = json.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t value_pos = colon_pos + 1;
    while (value_pos < json.size() && std::isspace(static_cast<unsigned char>(json[value_pos]))) {
        ++value_pos;
    }

    const std::size_t begin_pos = value_pos;
    while (value_pos < json.size() && std::isdigit(static_cast<unsigned char>(json[value_pos]))) {
        ++value_pos;
    }

    if (begin_pos == value_pos) {
        return std::nullopt;
    }

    try {
        return static_cast<std::uint64_t>(std::stoull(json.substr(begin_pos, value_pos - begin_pos)));
    } catch (...) {
        return std::nullopt;
    }
}

std::string extract_json_object_text(std::string value) {
    value = trim_copy_local(std::move(value));
    const std::size_t begin = value.find('{');
    const std::size_t end = value.rfind('}');
    if (begin == std::string::npos || end == std::string::npos || begin >= end) {
        return "";
    }
    return value.substr(begin, end - begin + 1);
}

std::optional<std::string> extract_chat_stream_delta_text(const std::string& json) {
    const std::size_t delta_pos = json.find("\"delta\"");
    if (delta_pos == std::string::npos) {
        return std::nullopt;
    }

    const auto content = extract_json_string_by_key(json, "content", delta_pos);
    if (content.has_value() && !trim_copy_local(content.value()).empty()) {
        return content;
    }

    const auto text = extract_json_string_by_key(json, "text", delta_pos);
    if (text.has_value() && !trim_copy_local(text.value()).empty()) {
        return text;
    }

    return std::nullopt;
}

bool emit_text_in_chunks(const std::string& text, const std::function<bool(const std::string&)>& on_delta) {
    if (!on_delta) {
        return true;
    }

    const std::size_t chunk_size = text.size() > 240 ? 24 : 16;
    for (std::size_t offset = 0; offset < text.size(); offset += chunk_size) {
        if (!on_delta(text.substr(offset, chunk_size))) {
            return false;
        }
    }

    return true;
}

}  // namespace

ConsultationService::ConsultationService(MySqlClient* database_client)
    : database_client_(database_client),
      using_mysql_(database_client_ != nullptr && database_client_->available()),
      sessions_file_path_(persistence::data_file_path("consultation_sessions.tsv")),
      messages_file_path_(persistence::data_file_path("consultation_messages.tsv")),
      api_key_(read_env_value("OPENAI_API_KEY")),
      responses_url_(resolve_responses_url()),
      chat_completions_url_(resolve_chat_completions_url()),
      api_format_(resolve_api_format()),
      model_name_(fallback_text(read_env_value("OPENAI_MODEL"), "gpt-4.1")),
      timeout_seconds_(resolve_timeout_seconds()) {
    if (using_mysql_) {
        if (ensure_mysql_schema() && load_sessions_from_mysql()) {
            return;
        }
        using_mysql_ = false;
    }

    load_persisted_state();
}

std::optional<ConsultationContext> ConsultationService::build_context(
    CompetitionRepository& repository,
    std::uint64_t competition_id,
    std::string& error_message
) const {
    const auto competition = repository.find_by_id(competition_id, true);
    if (!competition.has_value()) {
        error_message = u8"未找到对应竞赛。";
        return std::nullopt;
    }

    ConsultationContext context;
    context.competition = competition.value();
    context.welcome_message = build_welcome_message(context.competition);
    context.example_questions = build_example_questions(context.competition);
    return context;
}

std::optional<ConsultationReply> ConsultationService::ask(
    CompetitionRepository& repository,
    std::optional<std::uint64_t> user_id,
    std::uint64_t competition_id,
    const std::string& session_id,
    const std::vector<ConsultationMessage>& history_messages,
    const std::string& question,
    const std::string& response_mode,
    std::string& error_message
) {
    const auto competition = repository.find_by_id(competition_id, true);
    if (!competition.has_value()) {
        error_message = u8"未找到对应竞赛。";
        return std::nullopt;
    }

    if (api_key_.empty()) {
        error_message = u8"后端未配置 AI 接口密钥，请先设置 OPENAI_API_KEY。";
        return std::nullopt;
    }

    const std::string normalized_question = trim_copy(question);
    if (normalized_question.empty()) {
        error_message = u8"请输入咨询问题。";
        return std::nullopt;
    }

    std::string normalized_session_id = normalize_session_id(session_id);
    const std::vector<ConsultationMessage> normalized_history = normalize_history_messages(history_messages);
    ConsultationSession* session = nullptr;

    if (!normalized_session_id.empty()) {
        const auto existing = sessions_.find(normalized_session_id);
        if (existing != sessions_.end()) {
            if (existing->second.competition_id != competition_id) {
                error_message = u8"当前会话与所选竞赛不匹配。";
                return std::nullopt;
            }
            session = &existing->second;
        }
    }

    if (session == nullptr) {
        normalized_session_id = next_session_id_text();
        ConsultationSession new_session;
        new_session.session_id = normalized_session_id;
        new_session.competition_id = competition_id;
        new_session.update_time = current_timestamp();
        new_session.messages = normalized_history;
        sessions_[normalized_session_id] = new_session;
        session = &sessions_[normalized_session_id];
    } else if (!normalized_history.empty() && normalized_history.size() > session->messages.size()) {
        session->messages = normalized_history;
        session->update_time = current_timestamp();
    }

    ConsultationMessage user_message;
    user_message.role = "user";
    user_message.content = normalized_question;
    user_message.create_time = current_timestamp();
    session->messages.push_back(user_message);

    const auto answer = request_openai_answer(competition.value(), session->messages, error_message);
    if (!answer.has_value()) {
        session->messages.pop_back();
        return std::nullopt;
    }

    ConsultationMessage assistant_message;
    assistant_message.role = "assistant";
    assistant_message.content = answer.value();
    assistant_message.create_time = current_timestamp();
    session->messages.push_back(assistant_message);
    session->update_time = assistant_message.create_time;

    ConsultationReply reply;
    reply.session_id = normalized_session_id;
    reply.competition = competition.value();
    reply.question = normalized_question;
    reply.answer = answer.value();
    reply.response_mode = normalize_mode(response_mode);
    reply.follow_up_questions = build_follow_up_questions(competition.value());
    reply.messages = session->messages;
    reply.update_time = session->update_time;

    if (using_mysql_) {
        if (!persist_consultation_record(user_id, competition.value(), reply)) {
            error_message = database_client_->last_error();
            return std::nullopt;
        }
    } else {
        persist();
    }

    return reply;
}

std::optional<ConsultationReply> ConsultationService::ask_stream(
    CompetitionRepository& repository,
    std::optional<std::uint64_t> user_id,
    std::uint64_t competition_id,
    const std::string& session_id,
    const std::vector<ConsultationMessage>& history_messages,
    const std::string& question,
    const std::function<bool(const std::string&, const std::string&)>& emit_event,
    std::string& error_message
) {
    const auto competition = repository.find_by_id(competition_id, true);
    if (!competition.has_value()) {
        error_message = u8"未找到对应竞赛。";
        return std::nullopt;
    }

    if (api_key_.empty()) {
        error_message = u8"后端未配置 AI 接口密钥，请先设置 OPENAI_API_KEY。";
        return std::nullopt;
    }

    const std::string normalized_question = trim_copy(question);
    if (normalized_question.empty()) {
        error_message = u8"请输入咨询问题。";
        return std::nullopt;
    }

    std::string normalized_session_id = normalize_session_id(session_id);
    const std::vector<ConsultationMessage> normalized_history = normalize_history_messages(history_messages);
    ConsultationSession* session = nullptr;

    if (!normalized_session_id.empty()) {
        const auto existing = sessions_.find(normalized_session_id);
        if (existing != sessions_.end()) {
            if (existing->second.competition_id != competition_id) {
                error_message = u8"当前会话与所选竞赛不匹配。";
                return std::nullopt;
            }
            session = &existing->second;
        }
    }

    if (session == nullptr) {
        normalized_session_id = next_session_id_text();
        ConsultationSession new_session;
        new_session.session_id = normalized_session_id;
        new_session.competition_id = competition_id;
        new_session.update_time = current_timestamp();
        new_session.messages = normalized_history;
        sessions_[normalized_session_id] = new_session;
        session = &sessions_[normalized_session_id];
    } else if (!normalized_history.empty() && normalized_history.size() > session->messages.size()) {
        session->messages = normalized_history;
        session->update_time = current_timestamp();
    }

    ConsultationMessage user_message;
    user_message.role = "user";
    user_message.content = normalized_question;
    user_message.create_time = current_timestamp();
    session->messages.push_back(user_message);

    if (emit_event) {
        if (!emit_event("start", "{\"session_id\":" + quote_json_text(normalized_session_id) + "}")) {
            session->messages.pop_back();
            error_message = u8"流式连接已断开。";
            return std::nullopt;
        }
    }

    const auto answer = request_openai_answer_stream(
        competition.value(),
        session->messages,
        [&](const std::string& delta) {
            if (!emit_event) {
                return true;
            }
            return emit_event("delta", "{\"text\":" + quote_json_text(delta) + "}");
        },
        error_message
    );
    if (!answer.has_value()) {
        session->messages.pop_back();
        return std::nullopt;
    }

    ConsultationMessage assistant_message;
    assistant_message.role = "assistant";
    assistant_message.content = answer.value();
    assistant_message.create_time = current_timestamp();
    session->messages.push_back(assistant_message);
    session->update_time = assistant_message.create_time;

    ConsultationReply reply;
    reply.session_id = normalized_session_id;
    reply.competition = competition.value();
    reply.question = normalized_question;
    reply.answer = answer.value();
    reply.response_mode = "stream";
    reply.follow_up_questions = build_follow_up_questions(competition.value());
    reply.messages = session->messages;
    reply.update_time = session->update_time;

    if (using_mysql_) {
        if (!persist_consultation_record(user_id, competition.value(), reply)) {
            error_message = database_client_->last_error();
            session->messages.pop_back();
            session->messages.pop_back();
            return std::nullopt;
        }
    } else {
        persist();
    }

    if (emit_event) {
        if (!emit_event("done", consultation_reply_to_json(reply))) {
            error_message = u8"流式连接已断开。";
            return std::nullopt;
        }
    }

    return reply;
}

std::optional<NavigationIntent> ConsultationService::interpret_navigation(
    CompetitionRepository& repository,
    const std::string& command,
    std::string& error_message
) const {
    const std::string normalized_command = trim_copy(command);
    if (normalized_command.empty()) {
        error_message = u8"请输入导航指令。";
        return std::nullopt;
    }

    if (!api_key_.empty()) {
        const HttpResult result = send_json_request(
            chat_completions_url_,
            api_key_,
            build_navigation_request_json(repository, normalized_command),
            timeout_seconds_
        );

        if (result.error_message.empty() && result.status_code >= 200 && result.status_code < 300) {
            const auto ai_text = extract_chat_output_text(result.body);
            if (ai_text.has_value()) {
                const std::string json_text = extract_json_object_text(ai_text.value());
                if (!json_text.empty()) {
                    NavigationIntent intent;
                    intent.action = trim_copy(extract_json_string_by_key(json_text, "action").value_or(""));
                    intent.target_page = trim_copy(extract_json_string_by_key(json_text, "target_page").value_or(""));
                    intent.competition_name = trim_copy(extract_json_string_by_key(json_text, "competition_name").value_or(""));
                    intent.keyword = trim_copy(extract_json_string_by_key(json_text, "keyword").value_or(""));
                    intent.major = trim_copy(extract_json_string_by_key(json_text, "major").value_or(""));
                    intent.competition_level = trim_copy(extract_json_string_by_key(json_text, "competition_level").value_or(""));
                    intent.signup_status = trim_copy(extract_json_string_by_key(json_text, "signup_status").value_or(""));
                    intent.category_name = trim_copy(extract_json_string_by_key(json_text, "category_name").value_or(""));
                    intent.feedback_message = trim_copy(extract_json_string_by_key(json_text, "feedback_message").value_or(""));
                    intent.competition_id = extract_json_u64_by_key(json_text, "competition_id").value_or(0);
                    intent.used_ai = !intent.action.empty();
                    resolve_navigation_intent(repository, intent);
                    if (!intent.action.empty()) {
                        return intent;
                    }
                }
            }
        }
    }

    NavigationIntent intent = build_navigation_intent_by_heuristic(normalized_command);
    resolve_navigation_intent(repository, intent);
    if (intent.action.empty()) {
        intent.action = "unknown";
    }
    if (intent.feedback_message.empty()) {
        intent.feedback_message = u8"已按你的输入尝试处理指令。";
    }
    return intent;
}

ConsultationRecordsPayload ConsultationService::build_records_payload(CompetitionRepository& repository) const {
    ConsultationRecordsPayload payload;
    std::vector<const ConsultationSession*> ordered_sessions;
    ordered_sessions.reserve(sessions_.size());

    for (const auto& entry : sessions_) {
        ordered_sessions.push_back(&entry.second);
    }

    std::sort(ordered_sessions.begin(), ordered_sessions.end(), [](const ConsultationSession* left, const ConsultationSession* right) {
        return left->update_time > right->update_time;
    });

    std::unordered_set<std::uint64_t> competition_ids;
    std::unordered_map<std::string, ConsultationCategoryStat> category_stats;

    for (const ConsultationSession* session : ordered_sessions) {
        ConsultationRecordItem item;
        item.session_id = session->session_id;
        item.competition_id = session->competition_id;
        item.message_count = session->messages.size();
        item.update_time = session->update_time;

        const auto competition = repository.find_by_id(session->competition_id, true);
        if (competition.has_value()) {
            item.competition_name = competition->competition_name;
            item.category_name = fallback_text(competition->category_name, "Uncategorized");
        } else {
            item.competition_name = "Unknown competition";
            item.category_name = "Uncategorized";
        }

        for (auto it = session->messages.rbegin(); it != session->messages.rend(); ++it) {
            if (item.latest_question.empty() && it->role == "user") {
                item.latest_question = it->content;
            }
            if (item.latest_answer.empty() && it->role == "assistant") {
                item.latest_answer = it->content;
            }
            if (!item.latest_question.empty() && !item.latest_answer.empty()) {
                break;
            }
        }

        payload.items.push_back(item);
        payload.overview.total_messages += item.message_count;
        competition_ids.insert(item.competition_id);

        ConsultationCategoryStat& category_stat = category_stats[item.category_name];
        category_stat.category_name = item.category_name;
        category_stat.session_count += 1;
        category_stat.message_count += item.message_count;
    }

    payload.overview.total_sessions = payload.items.size();
    payload.overview.competition_total = competition_ids.size();

    payload.hot_categories.reserve(category_stats.size());
    for (const auto& entry : category_stats) {
        payload.hot_categories.push_back(entry.second);
    }

    std::sort(payload.hot_categories.begin(), payload.hot_categories.end(), [](const ConsultationCategoryStat& left, const ConsultationCategoryStat& right) {
        if (left.session_count != right.session_count) {
            return left.session_count > right.session_count;
        }
        if (left.message_count != right.message_count) {
            return left.message_count > right.message_count;
        }
        return left.category_name < right.category_name;
    });

    payload.overview.hot_category_total = payload.hot_categories.size();
    return payload;
}

std::string ConsultationService::current_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

std::string ConsultationService::trim_copy(std::string value) {
    return trim_copy_local(std::move(value));
}

std::string ConsultationService::to_lower(std::string value) {
    return to_lower_copy(std::move(value));
}

std::string ConsultationService::normalize_mode(std::string mode) {
    const std::string normalized = to_lower(trim_copy(std::move(mode)));
    return normalized == "stream" ? "stream" : "normal";
}

std::string ConsultationService::normalize_session_id(std::string session_id) {
    session_id = trim_copy(std::move(session_id));
    if (session_id.empty()) {
        return "";
    }

    std::string result;
    result.reserve(session_id.size());
    for (unsigned char ch : session_id) {
        if (std::isalnum(ch) || ch == '-' || ch == '_') {
            result.push_back(static_cast<char>(ch));
        }
    }

    if (result.size() > 64) {
        result.resize(64);
    }

    return result;
}

std::string ConsultationService::fallback_text(const std::string& value, const std::string& fallback) {
    return trim_copy(value).empty() ? fallback : value;
}

std::string ConsultationService::read_env_value(const char* name) {
    const char* raw = std::getenv(name);
    return raw == nullptr ? "" : trim_copy(raw);
}

std::string ConsultationService::resolve_responses_url() {
    const std::string explicit_url = read_env_value("OPENAI_RESPONSES_URL");
    if (!explicit_url.empty()) {
        return explicit_url;
    }

    const std::string base_url = normalize_base_url(read_env_value("OPENAI_BASE_URL"));
    if (base_url.empty()) {
        return "https://api.openai.com/v1/responses";
    }

    return base_url + "/responses";
}

std::string ConsultationService::resolve_chat_completions_url() {
    const std::string explicit_url = read_env_value("OPENAI_CHAT_COMPLETIONS_URL");
    if (!explicit_url.empty()) {
        return explicit_url;
    }

    const std::string base_url = normalize_base_url(read_env_value("OPENAI_BASE_URL"));
    if (base_url.empty()) {
        return "https://api.openai.com/v1/chat/completions";
    }

    return base_url + "/chat/completions";
}

std::string ConsultationService::resolve_api_format() {
    const std::string raw = to_lower(trim_copy(read_env_value("OPENAI_API_FORMAT")));
    if (raw == "responses") {
        return "responses";
    }
    if (raw == "chat" || raw == "chat-completions" || raw == "chat_completions") {
        return "chat";
    }
    return "auto";
}

int ConsultationService::resolve_timeout_seconds() {
    const std::string raw = read_env_value("OPENAI_TIMEOUT_SECONDS");
    if (raw.empty()) {
        return 60;
    }

    try {
        const int value = std::stoi(raw);
        return value > 0 ? value : 60;
    } catch (...) {
        return 60;
    }
}

std::vector<ConsultationMessage> ConsultationService::normalize_history_messages(const std::vector<ConsultationMessage>& history_messages) {
    std::vector<ConsultationMessage> normalized;
    normalized.reserve(history_messages.size());

    for (const ConsultationMessage& raw_message : history_messages) {
        const std::string role = to_lower(trim_copy(raw_message.role));
        const std::string content = trim_copy(raw_message.content);
        if ((role != "user" && role != "assistant") || content.empty()) {
            continue;
        }

        ConsultationMessage message;
        message.role = role;
        message.content = content;
        message.create_time = trim_copy(raw_message.create_time);
        normalized.push_back(message);
    }

    constexpr std::size_t kMaxHistoryMessages = 12;
    if (normalized.size() > kMaxHistoryMessages) {
        normalized.erase(normalized.begin(), normalized.end() - static_cast<std::ptrdiff_t>(kMaxHistoryMessages));
    }

    return normalized;
}

bool ConsultationService::load_persisted_state() {
    if (using_mysql_) {
        return load_sessions_from_mysql();
    }

    sessions_.clear();

    if (!persistence::file_exists(sessions_file_path_)) {
        next_session_id_ = 1;
        return false;
    }

    std::vector<std::string> session_lines;
    if (!persistence::read_lines(sessions_file_path_, session_lines)) {
        next_session_id_ = 1;
        return false;
    }

    std::vector<std::string> message_lines;
    persistence::read_lines(messages_file_path_, message_lines);

    std::vector<std::string> fields;
    std::uint64_t max_session_number = 0;

    for (const std::string& line : session_lines) {
        if (line.empty()) {
            continue;
        }

        if (!persistence::split_fields(line, fields) || fields.size() != 3) {
            sessions_.clear();
            next_session_id_ = 1;
            return false;
        }

        const auto competition_id = persistence::parse_u64(fields[1]);
        if (!competition_id.has_value()) {
            sessions_.clear();
            next_session_id_ = 1;
            return false;
        }

        ConsultationSession session;
        session.session_id = fields[0];
        session.competition_id = competition_id.value();
        session.update_time = fields[2];
        sessions_[session.session_id] = session;

        if (session.session_id.rfind("session-", 0) == 0) {
            const auto numeric_id = persistence::parse_u64(session.session_id.substr(8));
            if (numeric_id.has_value()) {
                max_session_number = std::max(max_session_number, numeric_id.value());
            }
        }
    }

    for (const std::string& line : message_lines) {
        if (line.empty()) {
            continue;
        }

        if (!persistence::split_fields(line, fields) || fields.size() != 4) {
            sessions_.clear();
            next_session_id_ = 1;
            return false;
        }

        const auto session_it = sessions_.find(fields[0]);
        if (session_it == sessions_.end()) {
            continue;
        }

        ConsultationMessage message;
        message.role = fields[1];
        message.content = fields[2];
        message.create_time = fields[3];
        session_it->second.messages.push_back(message);
    }

    next_session_id_ = max_session_number + 1;
    if (next_session_id_ == 0) {
        next_session_id_ = 1;
    }
    return true;
}

bool ConsultationService::persist() const {
    if (using_mysql_) {
        return true;
    }

    std::vector<const ConsultationSession*> ordered_sessions;
    ordered_sessions.reserve(sessions_.size());

    for (const auto& entry : sessions_) {
        ordered_sessions.push_back(&entry.second);
    }

    std::sort(ordered_sessions.begin(), ordered_sessions.end(), [](const ConsultationSession* left, const ConsultationSession* right) {
        return left->session_id < right->session_id;
    });

    std::vector<std::string> session_lines;
    std::vector<std::string> message_lines;
    session_lines.reserve(ordered_sessions.size());

    for (const ConsultationSession* session : ordered_sessions) {
        session_lines.push_back(persistence::join_fields({
            session->session_id,
            std::to_string(session->competition_id),
            session->update_time
        }));

        for (const ConsultationMessage& message : session->messages) {
            message_lines.push_back(persistence::join_fields({
                session->session_id,
                message.role,
                message.content,
                message.create_time
            }));
        }
    }

    return persistence::write_lines(sessions_file_path_, session_lines)
        && persistence::write_lines(messages_file_path_, message_lines);
}

bool ConsultationService::ensure_mysql_schema() {
    return database_client_->execute(
        "CREATE TABLE IF NOT EXISTS consult_record ("
        "record_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "user_id BIGINT UNSIGNED DEFAULT NULL,"
        "competition_id BIGINT UNSIGNED DEFAULT NULL,"
        "session_id VARCHAR(64) DEFAULT NULL,"
        "question_text TEXT NOT NULL,"
        "answer TEXT DEFAULT NULL,"
        "consult_type VARCHAR(32) DEFAULT NULL,"
        "response_mode VARCHAR(16) DEFAULT NULL,"
        "prompt_context TEXT DEFAULT NULL,"
        "consult_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (record_id),"
        "KEY idx_consult_record_session_time (session_id, consult_time),"
        "KEY idx_consult_record_competition_time (competition_id, consult_time),"
        "CONSTRAINT fk_consult_record_user FOREIGN KEY (user_id) REFERENCES user_info (user_id) ON DELETE SET NULL ON UPDATE CASCADE,"
        "CONSTRAINT fk_consult_record_competition FOREIGN KEY (competition_id) REFERENCES competition_info (competition_id) ON DELETE SET NULL ON UPDATE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    );
}

bool ConsultationService::load_sessions_from_mysql() {
    sessions_.clear();
    next_session_id_ = 1;

    const auto result = database_client_->query(
        "SELECT session_id, IFNULL(competition_id, 0) AS competition_id, "
        "question_text, IFNULL(answer, '') AS answer, "
        "DATE_FORMAT(consult_time, '%Y-%m-%d %H:%i:%s') AS consult_time "
        "FROM consult_record "
        "WHERE session_id IS NOT NULL AND session_id <> '' "
        "ORDER BY consult_time ASC, record_id ASC"
    );

    if (!result.has_value()) {
        return false;
    }

    std::uint64_t max_session_number = 0;

    for (const auto& row : result->rows) {
        const std::string session_id = row.at("session_id");
        ConsultationSession& session = sessions_[session_id];
        if (session.session_id.empty()) {
            session.session_id = session_id;
            session.competition_id = parse_u64_or_zero(row.at("competition_id"));
        }

        ConsultationMessage user_message;
        user_message.role = "user";
        user_message.content = row.at("question_text");
        user_message.create_time = row.at("consult_time");
        if (!trim_copy(user_message.content).empty()) {
            session.messages.push_back(user_message);
        }

        ConsultationMessage assistant_message;
        assistant_message.role = "assistant";
        assistant_message.content = row.at("answer");
        assistant_message.create_time = row.at("consult_time");
        if (!trim_copy(assistant_message.content).empty()) {
            session.messages.push_back(assistant_message);
        }

        session.update_time = row.at("consult_time");

        if (session_id.rfind("session-", 0) == 0) {
            const auto numeric_id = persistence::parse_u64(session_id.substr(8));
            if (numeric_id.has_value()) {
                max_session_number = std::max(max_session_number, numeric_id.value());
            }
        }
    }

    next_session_id_ = max_session_number + 1;
    return true;
}

bool ConsultationService::persist_consultation_record(
    std::optional<std::uint64_t> user_id,
    const Competition& competition,
    const ConsultationReply& reply
) const {
    const std::string user_id_sql = user_id.has_value() ? std::to_string(user_id.value()) : "NULL";
    const std::string prompt_context = build_competition_brief(competition);
    const std::string response_mode_text = reply.response_mode == "stream" ? "SSE" : "JSON";

    return database_client_->execute(
        "INSERT INTO consult_record (user_id, competition_id, session_id, question_text, answer, consult_type, response_mode, prompt_context, consult_time) VALUES ("
        + user_id_sql + ","
        + std::to_string(competition.competition_id) + ","
        + database_client_->quote(reply.session_id) + ","
        + database_client_->quote(reply.question) + ","
        + database_client_->quote(reply.answer) + ","
        + database_client_->quote("general") + ","
        + database_client_->quote(response_mode_text) + ","
        + database_client_->quote(prompt_context) + ","
        + database_client_->quote(reply.update_time) + ")"
    );
}

std::string ConsultationService::next_session_id_text() {
    while (true) {
        const std::string candidate = "session-" + std::to_string(next_session_id_++);
        if (sessions_.find(candidate) == sessions_.end()) {
            return candidate;
        }
    }
}

std::vector<std::string> ConsultationService::build_example_questions(const Competition& competition) const {
    return {
        std::string(u8"这个竞赛适合什么基础的同学？"),
        std::string(u8"如果我现在开始准备，应该先做哪些事情？"),
        std::string(u8"它和其他") + fallback_text(competition.category_name, std::string(u8"同类")) + std::string(u8"竞赛相比有什么特点？")
    };
}

std::string ConsultationService::build_welcome_message(const Competition& competition) const {
    return std::string(u8"你好，我已经关联到“")
        + fallback_text(competition.competition_name, std::string(u8"当前竞赛"))
        + std::string(u8"”的竞赛信息了。你可以继续问我它是否适合你、准备重点、组队建议，或者和同类竞赛的区别。");
}

std::vector<std::string> ConsultationService::build_follow_up_questions(const Competition& competition) const {
    return {
        std::string(u8"如果我告诉你我的专业和年级，你能判断我适不适合参加吗？"),
        std::string(u8"如果我现在开始准备，第一步最应该做什么？"),
        std::string(u8"你能帮我把它和另一个") + fallback_text(competition.category_name, std::string(u8"竞赛")) + std::string(u8"做一下对比吗？")
    };
}

std::string ConsultationService::build_competition_brief(const Competition& competition) const {
    std::vector<std::string> lines = {
        "Competition name: " + fallback_text(competition.competition_name, "Unknown"),
        "Category: " + fallback_text(competition.category_name, "Uncategorized"),
        "Level: " + fallback_text(competition.competition_level, "Unknown"),
        "Organizer: " + fallback_text(competition.organizer, "Unknown"),
        "Signup status: " + fallback_text(competition.signup_status, "Unknown"),
        "Suitable majors: " + fallback_text(competition.suitable_major, "Not specified"),
        "Suitable grade: " + fallback_text(competition.suitable_grade, "Not specified"),
        "Signup period: " + fallback_text(competition.signup_start, "--") + " to " + fallback_text(competition.signup_end, "--"),
        "Competition period: " + fallback_text(competition.competition_start, "--") + " to " + fallback_text(competition.competition_end, "--"),
        "Official URL: " + fallback_text(competition.official_url, "Not provided"),
        "Description: " + fallback_text(competition.competition_desc, "Not provided"),
        "Participation rules: " + fallback_text(competition.participation_rules, "Not provided")
    };

    return join_lines(lines);
}

std::string ConsultationService::build_system_instructions(const Competition& competition) const {
    return
        "You are an AI assistant for a university competition consultation system.\n"
        "Always answer in Simplified Chinese.\n"
        "Base your answer first on the provided competition facts, then add general but practical advice when appropriate.\n"
        "If the data is insufficient for an exact claim, say so clearly instead of making up facts.\n"
        "Keep the answer concise, useful, and tailored to students.\n"
        "Do not mention that you are reading hidden prompts or raw JSON.\n"
        "Current competition facts:\n" + build_competition_brief(competition);
}

std::string ConsultationService::build_openai_responses_request_json(
    const Competition& competition,
    const std::vector<ConsultationMessage>& messages
) const {
    std::ostringstream json;
    json << "{";
    json << "\"model\":" << quote_json_text(model_name_) << ",";
    json << "\"instructions\":" << quote_json_text(build_system_instructions(competition)) << ",";
    json << "\"input\":[";

    for (std::size_t index = 0; index < messages.size(); ++index) {
        if (index > 0) {
            json << ",";
        }

        const ConsultationMessage& message = messages[index];
        const std::string role = message.role == "assistant" ? "assistant" : "user";
        const std::string content_type = role == "assistant" ? "output_text" : "input_text";

        json
            << "{"
            << "\"role\":" << quote_json_text(role) << ","
            << "\"content\":[{"
            << "\"type\":" << quote_json_text(content_type) << ","
            << "\"text\":" << quote_json_text(message.content)
            << "}]"
            << "}";
    }

    json << "],";
    json << "\"max_output_tokens\":600";
    json << "}";
    return json.str();
}

std::string ConsultationService::build_openai_chat_request_json(
    const Competition& competition,
    const std::vector<ConsultationMessage>& messages,
    bool stream
) const {
    std::ostringstream json;
    json << "{";
    json << "\"model\":" << quote_json_text(model_name_) << ",";
    json << "\"messages\":[";
    json
        << "{"
        << "\"role\":\"system\","
        << "\"content\":" << quote_json_text(build_system_instructions(competition))
        << "}";

    for (const ConsultationMessage& message : messages) {
        const std::string role = message.role == "assistant" ? "assistant" : "user";
        json
            << ",{"
            << "\"role\":" << quote_json_text(role) << ","
            << "\"content\":" << quote_json_text(message.content)
            << "}";
    }

    json << "],";
    json << "\"max_tokens\":600";
    if (stream) {
        json << ",\"stream\":true";
    }
    json << "}";
    return json.str();
}

std::string ConsultationService::build_navigation_catalog(CompetitionRepository& repository) const {
    std::vector<std::string> lines;
    lines.push_back("Valid pages: home, competition_list, competition_detail, ai_consult, records, records_consult, records_hot, admin");
    lines.push_back("Valid actions: open_competition_list, search_competition, open_competition_detail, open_ai_consult, open_records, open_admin, view_consult_records, view_hot_stats, unknown");
    lines.push_back("Valid signup_status values: Open, Warmup, Closed, Ended, Draft");
    lines.push_back("Valid competition_level values: National, National A, Provincial, International, School");

    const std::vector<Category> categories = repository.list_categories();
    if (!categories.empty()) {
        std::vector<std::string> category_names;
        category_names.reserve(categories.size());
        for (const Category& category : categories) {
            category_names.push_back(category.category_name);
        }
        lines.push_back("Available categories: " + join_lines(category_names));
    }

    CompetitionFilter filter;
    filter.include_hidden = true;
    const std::vector<Competition> competitions = repository.list(filter);
    lines.push_back("Available competitions:");
    for (const Competition& competition : competitions) {
        lines.push_back(
            "- " + fallback_text(competition.competition_name, "Unknown")
            + " | category=" + fallback_text(competition.category_name, "Uncategorized")
            + " | level=" + fallback_text(competition.competition_level, "Unknown")
            + " | status=" + fallback_text(competition.signup_status, "Unknown")
        );
    }

    return join_lines(lines);
}

std::string ConsultationService::build_navigation_system_instructions(CompetitionRepository& repository) const {
    return
        "You are the navigation parser for a university competition web system.\n"
        "You must output one JSON object only, without markdown or explanation.\n"
        "All JSON string values must be plain text.\n"
        "The field action must be one of: open_competition_list, search_competition, open_competition_detail, open_ai_consult, open_records, open_admin, view_consult_records, view_hot_stats, unknown.\n"
        "The field target_page must be one of: home, competition_list, competition_detail, ai_consult, records, records_consult, records_hot, admin.\n"
        "If the user asks to search or filter competitions, use action=search_competition.\n"
        "If the user mentions a specific competition, put its official name into competition_name when possible.\n"
        "Map filters to exact English dataset values when possible, especially category_name, competition_level and signup_status.\n"
        "If a field is not needed, output an empty string, and output competition_id as 0.\n"
        "feedback_message must be a short Simplified Chinese sentence for UI feedback.\n"
        "Output JSON fields exactly: action, target_page, competition_id, competition_name, keyword, major, competition_level, signup_status, category_name, feedback_message.\n"
        "Catalog:\n" + build_navigation_catalog(repository);
}

std::string ConsultationService::build_navigation_request_json(CompetitionRepository& repository, const std::string& command) const {
    std::ostringstream json;
    json << "{";
    json << "\"model\":" << quote_json_text(model_name_) << ",";
    json << "\"messages\":[";
    json
        << "{"
        << "\"role\":\"system\","
        << "\"content\":" << quote_json_text(build_navigation_system_instructions(repository))
        << "},"
        << "{"
        << "\"role\":\"user\","
        << "\"content\":" << quote_json_text("用户指令：" + command)
        << "}";
    json << "],";
    json << "\"max_tokens\":280";
    json << "}";
    return json.str();
}

NavigationIntent ConsultationService::build_navigation_intent_by_heuristic(const std::string& command) const {
    NavigationIntent intent;
    const std::string lowered = to_lower(command);

    const auto contains = [&](const std::string& token) {
        return lowered.find(to_lower(token)) != std::string::npos;
    };

    if (contains("人工智能") || contains("ai")) {
        intent.category_name = "Artificial Intelligence";
        if (intent.feedback_message.empty()) {
            intent.feedback_message = u8"已识别为人工智能相关竞赛。";
        }
    }
    if (contains("数学建模")) {
        intent.category_name = "Mathematical Modeling";
    }
    if (contains("编程") || contains("程序设计")) {
        intent.category_name = "Programming";
    }
    if (contains("创新创业")) {
        intent.category_name = "Innovation and Entrepreneurship";
    }

    if (contains("人工智能专业")) {
        intent.major = "Artificial Intelligence";
    } else if (contains("数学")) {
        intent.major = "Mathematics";
    } else if (contains("设计")) {
        intent.major = "Design";
    } else if (contains("不限专业") || contains("全部专业")) {
        intent.major = "All majors";
    }

    if (contains("国际")) {
        intent.competition_level = "International";
    } else if (contains("国家级a") || contains("国a")) {
        intent.competition_level = "National A";
    } else if (contains("国家级") || contains("国级") || contains("国赛")) {
        intent.competition_level = "National";
    } else if (contains("省级")) {
        intent.competition_level = "Provincial";
    } else if (contains("校级")) {
        intent.competition_level = "School";
    }

    if (contains("报名中") || contains("可报名") || contains("正在报名")) {
        intent.signup_status = "Open";
    } else if (contains("预热")) {
        intent.signup_status = "Warmup";
    } else if (contains("已截止") || contains("截止")) {
        intent.signup_status = "Closed";
    } else if (contains("已结束") || contains("结束")) {
        intent.signup_status = "Ended";
    }

    if (contains("蓝桥")) {
        intent.competition_name = "Lanqiao Cup Software and Information Technology Competition";
        intent.keyword = "Lanqiao";
    } else if (contains("数学建模") || contains("cumcm")) {
        intent.competition_name = "National College Student Mathematical Contest in Modeling";
        intent.keyword = "Mathematical Modeling";
    } else if (contains("icpc")) {
        intent.competition_name = "ICPC International Collegiate Programming Contest";
        intent.keyword = "ICPC";
    } else if (contains("maker china")) {
        intent.competition_name = "Maker China University Innovation Competition";
        intent.keyword = "Maker China";
    } else if (contains("创新挑战") || contains("ai创新")) {
        intent.competition_name = "Global AI Innovation Challenge";
        intent.keyword = "AI Innovation";
    }

    if (contains("咨询记录")) {
        intent.action = "view_consult_records";
        intent.target_page = "records_consult";
        intent.feedback_message = u8"已为你打开咨询记录区域。";
        return intent;
    }

    if (contains("热门") || contains("热度统计") || contains("热门统计")) {
        intent.action = "view_hot_stats";
        intent.target_page = "records_hot";
        intent.feedback_message = u8"已为你打开热门统计区域。";
        return intent;
    }

    if (contains("记录") || contains("统计")) {
        intent.action = "open_records";
        intent.target_page = "records";
        intent.feedback_message = u8"已为你打开记录统计页。";
        return intent;
    }

    if (contains("后台") || contains("管理")) {
        intent.action = "open_admin";
        intent.target_page = "admin";
        intent.feedback_message = u8"正在前往后台管理页。";
        return intent;
    }

    if (contains("咨询") || contains("问答")) {
        intent.action = "open_ai_consult";
        intent.target_page = "ai_consult";
        if (intent.feedback_message.empty()) {
            intent.feedback_message = u8"正在为你打开 AI 咨询。";
        }
        return intent;
    }

    if (contains("详情") || contains("查看竞赛")) {
        intent.action = "open_competition_detail";
        intent.target_page = "competition_detail";
        if (intent.feedback_message.empty()) {
            intent.feedback_message = u8"正在为你打开竞赛详情。";
        }
        if (intent.keyword.empty()) {
            intent.keyword = command;
        }
        return intent;
    }

    if ((contains("打开") && contains("竞赛列表")) || contains("竞赛广场") || contains("首页")) {
        intent.action = "open_competition_list";
        intent.target_page = "competition_list";
        intent.feedback_message = u8"已为你打开竞赛列表。";
        return intent;
    }

    if (contains("搜索") || contains("查找") || contains("筛选") || contains("看看")) {
        intent.action = "search_competition";
        intent.target_page = "competition_list";
        if (intent.keyword.empty() && intent.category_name.empty()) {
            intent.keyword = command;
        }
        if (intent.feedback_message.empty()) {
            intent.feedback_message = u8"已按你的要求筛选竞赛。";
        }
        return intent;
    }

    intent.action = "search_competition";
    intent.target_page = "competition_list";
    if (intent.keyword.empty() && intent.category_name.empty()) {
        intent.keyword = command;
    }
    if (intent.feedback_message.empty()) {
        intent.feedback_message = u8"已根据你的输入尝试搜索竞赛。";
    }
    return intent;
}

void ConsultationService::resolve_navigation_intent(CompetitionRepository& repository, NavigationIntent& intent) const {
    if (intent.action.empty()) {
        intent.action = "unknown";
    }

    if (intent.target_page.empty()) {
        if (intent.action == "open_competition_list" || intent.action == "search_competition") {
            intent.target_page = "competition_list";
        } else if (intent.action == "open_competition_detail") {
            intent.target_page = "competition_detail";
        } else if (intent.action == "open_ai_consult") {
            intent.target_page = "ai_consult";
        } else if (intent.action == "open_records") {
            intent.target_page = "records";
        } else if (intent.action == "open_admin") {
            intent.target_page = "admin";
        } else if (intent.action == "view_consult_records") {
            intent.target_page = "records_consult";
        } else if (intent.action == "view_hot_stats") {
            intent.target_page = "records_hot";
        } else {
            intent.target_page = "home";
        }
    }

    if (intent.competition_id > 0) {
        const auto competition = repository.find_by_id(intent.competition_id, true);
        if (competition.has_value()) {
            if (intent.competition_name.empty()) {
                intent.competition_name = competition->competition_name;
            }
            if (intent.category_name.empty()) {
                intent.category_name = competition->category_name;
            }
        }
    }

    if (intent.competition_id == 0) {
        const std::string probe = !trim_copy(intent.competition_name).empty()
            ? intent.competition_name
            : intent.keyword;

        if (!trim_copy(probe).empty()) {
            CompetitionFilter filter;
            filter.include_hidden = true;
            const std::vector<Competition> competitions = repository.list(filter);

            int best_score = -1;
            std::optional<Competition> best_match;
            const std::string normalized_probe = to_lower(trim_copy(probe));

            for (const Competition& competition : competitions) {
                const std::string competition_name = to_lower(trim_copy(competition.competition_name));
                const std::string category_name = to_lower(trim_copy(competition.category_name));
                int score = -1;

                if (competition_name == normalized_probe) {
                    score = 100;
                } else if (competition_name.find(normalized_probe) != std::string::npos) {
                    score = 90;
                } else if (category_name == normalized_probe) {
                    score = 70;
                } else if (category_name.find(normalized_probe) != std::string::npos) {
                    score = 60;
                }

                if (score > best_score) {
                    best_score = score;
                    best_match = competition;
                }
            }

            if (best_match.has_value()) {
                intent.competition_id = best_match->competition_id;
                if (intent.competition_name.empty()) {
                    intent.competition_name = best_match->competition_name;
                }
                if (intent.category_name.empty()) {
                    intent.category_name = best_match->category_name;
                }
            }
        }
    }

    if ((intent.action == "open_competition_detail" || intent.action == "open_ai_consult") && intent.competition_id == 0) {
        intent.action = "search_competition";
        intent.target_page = "competition_list";
        if (intent.feedback_message.empty()) {
            intent.feedback_message = u8"没有精确定位到竞赛，先为你展示相关搜索结果。";
        }
    }

    if (intent.feedback_message.empty()) {
        if (intent.action == "open_competition_list") {
            intent.feedback_message = u8"已为你打开竞赛列表。";
        } else if (intent.action == "search_competition") {
            intent.feedback_message = u8"已按你的要求筛选竞赛。";
        } else if (intent.action == "open_competition_detail") {
            intent.feedback_message = u8"正在打开竞赛详情。";
        } else if (intent.action == "open_ai_consult") {
            intent.feedback_message = u8"正在打开 AI 咨询。";
        } else if (intent.action == "open_records") {
            intent.feedback_message = u8"已打开记录统计页。";
        } else if (intent.action == "open_admin") {
            intent.feedback_message = u8"正在前往后台管理页。";
        } else if (intent.action == "view_consult_records") {
            intent.feedback_message = u8"已打开咨询记录区域。";
        } else if (intent.action == "view_hot_stats") {
            intent.feedback_message = u8"已打开热门统计区域。";
        } else {
            intent.feedback_message = u8"暂时没有完全理解你的指令，可以换一种说法再试试。";
        }
    }
}

std::optional<std::string> ConsultationService::request_openai_answer(
    const Competition& competition,
    const std::vector<ConsultationMessage>& messages,
    std::string& error_message
) const {
    const auto normalize_text = [&](const std::optional<std::string>& text) -> std::optional<std::string> {
        if (!text.has_value()) {
            return std::nullopt;
        }

        const std::string normalized = trim_copy(text.value());
        if (normalized.empty()) {
            return std::nullopt;
        }

        return normalized;
    };

    const auto request_by_responses = [&]() -> HttpResult {
        return send_json_request(
            responses_url_,
            api_key_,
            build_openai_responses_request_json(competition, messages),
            timeout_seconds_
        );
    };

    const auto request_by_chat = [&]() -> HttpResult {
        return send_json_request(
            chat_completions_url_,
            api_key_,
            build_openai_chat_request_json(competition, messages, false),
            timeout_seconds_
        );
    };

    const auto parse_success_text = [&](const HttpResult& result, bool use_chat_format) -> std::optional<std::string> {
        const auto text = use_chat_format
            ? normalize_text(extract_chat_output_text(result.body))
            : normalize_text(extract_responses_output_text(result.body));

        if (text.has_value()) {
            return text;
        }

        error_message = u8"AI 接口返回了空内容。";
        return std::nullopt;
    };

    const auto handle_failure = [&](const HttpResult& result) -> std::optional<std::string> {
        if (!result.error_message.empty()) {
            error_message = result.error_message;
        } else {
            error_message = extract_error_message(result.body);
        }
        return std::nullopt;
    };

    if (api_format_ == "responses") {
        const HttpResult result = request_by_responses();
        if (!result.error_message.empty() || result.status_code < 200 || result.status_code >= 300) {
            return handle_failure(result);
        }
        return parse_success_text(result, false);
    }

    if (api_format_ == "chat") {
        const HttpResult result = request_by_chat();
        if (!result.error_message.empty() || result.status_code < 200 || result.status_code >= 300) {
            return handle_failure(result);
        }
        return parse_success_text(result, true);
    }

    const HttpResult responses_result = request_by_responses();
    if (responses_result.error_message.empty() && responses_result.status_code >= 200 && responses_result.status_code < 300) {
        return parse_success_text(responses_result, false);
    }

    if (!should_try_chat_fallback(responses_result)) {
        return handle_failure(responses_result);
    }

    const HttpResult chat_result = request_by_chat();
    if (!chat_result.error_message.empty() || chat_result.status_code < 200 || chat_result.status_code >= 300) {
        return handle_failure(chat_result);
    }

    return parse_success_text(chat_result, true);
}

std::optional<std::string> ConsultationService::request_openai_answer_stream(
    const Competition& competition,
    const std::vector<ConsultationMessage>& messages,
    const std::function<bool(const std::string&)>& on_delta,
    std::string& error_message
) const {
    const auto normalize_text = [&](const std::optional<std::string>& text) -> std::optional<std::string> {
        if (!text.has_value()) {
            return std::nullopt;
        }

        const std::string normalized = trim_copy(text.value());
        if (normalized.empty()) {
            return std::nullopt;
        }

        return normalized;
    };

    const auto fallback_one_shot = [&]() -> std::optional<std::string> {
        const auto answer = request_openai_answer(competition, messages, error_message);
        if (!answer.has_value()) {
            return std::nullopt;
        }

        if (!emit_text_in_chunks(answer.value(), on_delta)) {
            error_message = u8"流式连接已断开。";
            return std::nullopt;
        }

        return answer;
    };

    if (api_format_ == "responses") {
        return fallback_one_shot();
    }

    std::string streamed_answer;
    std::string pending_buffer;
    bool received_any_delta = false;

    const HttpResult stream_result = send_json_request_stream(
        chat_completions_url_,
        api_key_,
        build_openai_chat_request_json(competition, messages, true),
        timeout_seconds_,
        [&](const std::string& chunk) {
            pending_buffer += chunk;
            std::string normalized = pending_buffer;
            normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());

            std::size_t event_end = normalized.find("\n\n");
            while (event_end != std::string::npos) {
                const std::string block = normalized.substr(0, event_end);
                pending_buffer = normalized.substr(event_end + 2);
                normalized = pending_buffer;

                std::string event_name;
                std::string data_text;
                std::istringstream stream(block);
                std::string line;
                while (std::getline(stream, line)) {
                    line = trim_copy(line);
                    if (line.rfind("event:", 0) == 0) {
                        event_name = trim_copy(line.substr(6));
                    } else if (line.rfind("data:", 0) == 0) {
                        const std::string part = trim_copy(line.substr(5));
                        if (!data_text.empty()) {
                            data_text += "\n";
                        }
                        data_text += part;
                    }
                }

                if (data_text == "[DONE]") {
                    event_end = normalized.find("\n\n");
                    continue;
                }

                const auto delta_text = normalize_text(extract_chat_stream_delta_text(data_text));
                if (delta_text.has_value()) {
                    streamed_answer += delta_text.value();
                    received_any_delta = true;
                    if (!on_delta(delta_text.value())) {
                        return false;
                    }
                } else if (event_name == "error" && !data_text.empty()) {
                    error_message = extract_error_message(data_text);
                    return false;
                }

                event_end = normalized.find("\n\n");
            }

            return true;
        }
    );

    if (!stream_result.error_message.empty() && !received_any_delta) {
        error_message = stream_result.error_message;
        return fallback_one_shot();
    }

    if (stream_result.status_code < 200 || stream_result.status_code >= 300) {
        error_message = stream_result.error_message.empty()
            ? extract_error_message(stream_result.body)
            : stream_result.error_message;
        if (!received_any_delta) {
            return fallback_one_shot();
        }
        return std::nullopt;
    }

    if (received_any_delta) {
        const std::string normalized_answer = trim_copy(streamed_answer);
        if (!normalized_answer.empty()) {
            return normalized_answer;
        }
    }

    const auto chat_text = normalize_text(extract_chat_output_text(stream_result.body));
    if (chat_text.has_value()) {
        if (!emit_text_in_chunks(chat_text.value(), on_delta)) {
            error_message = u8"流式连接已断开。";
            return std::nullopt;
        }
        return chat_text;
    }

    error_message = stream_result.error_message.empty()
        ? u8"AI 接口未返回有效内容。"
        : stream_result.error_message;
    return fallback_one_shot();
}

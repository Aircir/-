#include "activity_service.h"
#include "auth_service.h"
#include "competition_repository.h"
#include "consultation_service.h"
#include "http_server.h"
#include "json_utils.h"
#include "mysql_client.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
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

std::string pick_request_value(const HttpRequest& request, const std::vector<std::string>& candidates) {
    for (const std::string& key : candidates) {
        const auto body_it = request.form_params.find(key);
        if (body_it != request.form_params.end() && !body_it->second.empty()) {
            return body_it->second;
        }

        const auto query_it = request.query_params.find(key);
        if (query_it != request.query_params.end() && !query_it->second.empty()) {
            return query_it->second;
        }
    }

    return "";
}

std::optional<std::uint64_t> parse_unsigned_id(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    for (unsigned char ch : value) {
        if (!std::isdigit(ch)) {
            return std::nullopt;
        }
    }

    try {
        return static_cast<std::uint64_t>(std::stoull(value));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> parse_int_value(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return std::nullopt;
    }
}

bool parse_bool_value(const std::string& value, bool default_value) {
    if (value.empty()) {
        return default_value;
    }

    const std::string normalized = to_lower(trim_copy(value));
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

std::string normalize_datetime_text(std::string value) {
    value = trim_copy(value);
    std::replace(value.begin(), value.end(), 'T', ' ');
    if (value.size() == 16) {
        value += ":00";
    }
    return value;
}

std::string normalized_path(std::string path) {
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

std::optional<std::uint64_t> extract_resource_id(const std::string& request_path, const std::string& prefix) {
    const std::string path = normalized_path(request_path);
    if (path.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    return parse_unsigned_id(path.substr(prefix.size()));
}

CompetitionFilter build_filter(const HttpRequest& request) {
    CompetitionFilter filter;
    filter.keyword = pick_request_value(request, {"keyword", "q"});
    filter.major = pick_request_value(request, {"major"});
    filter.competition_level = pick_request_value(request, {"competition_level", "level"});
    filter.signup_status = pick_request_value(request, {"signup_status", "signupStatus", "status"});
    filter.suitable_grade = pick_request_value(request, {"suitable_grade", "grade"});
    filter.category_name = pick_request_value(request, {"category_name", "category"});
    filter.include_hidden = parse_bool_value(pick_request_value(request, {"include_hidden", "includeHidden"}), false);

    const std::string category_id_text = pick_request_value(request, {"category_id", "categoryId"});
    filter.category_id = parse_unsigned_id(category_id_text);
    return filter;
}

CategoryDraft build_category_draft(const HttpRequest& request) {
    CategoryDraft draft;
    draft.category_name = trim_copy(pick_request_value(request, {"category_name", "name"}));
    draft.category_desc = trim_copy(pick_request_value(request, {"category_desc", "description"}));
    draft.sort_no = parse_int_value(pick_request_value(request, {"sort_no", "sortNo"})).value_or(0);
    return draft;
}

CompetitionDraft build_competition_draft(const HttpRequest& request) {
    CompetitionDraft draft;
    draft.category_id = parse_unsigned_id(pick_request_value(request, {"category_id", "categoryId"})).value_or(0);
    draft.competition_name = trim_copy(pick_request_value(request, {"competition_name", "name"}));
    draft.organizer = trim_copy(pick_request_value(request, {"organizer"}));
    draft.competition_level = trim_copy(pick_request_value(request, {"competition_level", "level"}));
    draft.signup_status = trim_copy(pick_request_value(request, {"signup_status", "status"}));
    draft.suitable_major = trim_copy(pick_request_value(request, {"suitable_major", "major"}));
    draft.suitable_grade = trim_copy(pick_request_value(request, {"suitable_grade", "grade"}));
    draft.signup_start = normalize_datetime_text(pick_request_value(request, {"signup_start"}));
    draft.signup_end = normalize_datetime_text(pick_request_value(request, {"signup_end"}));
    draft.competition_start = normalize_datetime_text(pick_request_value(request, {"competition_start"}));
    draft.competition_end = normalize_datetime_text(pick_request_value(request, {"competition_end"}));
    draft.official_url = trim_copy(pick_request_value(request, {"official_url"}));
    draft.competition_desc = trim_copy(pick_request_value(request, {"competition_desc", "description"}));
    draft.participation_rules = trim_copy(pick_request_value(request, {"participation_rules", "rules"}));
    draft.display_status = parse_bool_value(pick_request_value(request, {"display_status", "displayStatus"}), true);
    return draft;
}

std::optional<std::uint64_t> build_competition_id_from_request(const HttpRequest& request) {
    return parse_unsigned_id(pick_request_value(request, {"competition_id", "competitionId", "id"}));
}

std::string build_consultation_question(const HttpRequest& request) {
    return trim_copy(pick_request_value(request, {"question", "message", "prompt"}));
}

std::string build_consultation_mode(const HttpRequest& request) {
    return trim_copy(pick_request_value(request, {"response_mode", "responseMode", "mode"}));
}

std::string build_consultation_session_id(const HttpRequest& request) {
    return trim_copy(pick_request_value(request, {"session_id", "sessionId"}));
}

std::string build_navigation_command(const HttpRequest& request) {
    return trim_copy(pick_request_value(request, {"command", "query", "text"}));
}

std::size_t build_result_count(const HttpRequest& request) {
    const auto value = parse_unsigned_id(pick_request_value(request, {"result_count", "resultCount", "count"}));
    return value.has_value() ? static_cast<std::size_t>(value.value()) : 0;
}

std::string build_page_name(const HttpRequest& request, const std::vector<std::string>& candidates) {
    return trim_copy(pick_request_value(request, candidates));
}

std::string build_login_username(const HttpRequest& request) {
    return trim_copy(pick_request_value(request, {"username", "account"}));
}

std::string build_login_password(const HttpRequest& request) {
    return trim_copy(pick_request_value(request, {"password"}));
}

std::string extract_bearer_token(const HttpRequest& request) {
    const auto header = request.headers.find("authorization");
    if (header == request.headers.end()) {
        return "";
    }

    const std::string value = trim_copy(header->second);
    if (value.size() <= 7) {
        return "";
    }

    const std::string prefix = to_lower(value.substr(0, 7));
    if (prefix != "bearer ") {
        return "";
    }

    return trim_copy(value.substr(7));
}

std::optional<AuthUser> request_user(const HttpRequest& request, AuthService& auth_service) {
    return auth_service.authenticate(extract_bearer_token(request));
}

void skip_json_whitespace(const std::string& text, std::size_t& cursor) {
    while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) {
        ++cursor;
    }
}

std::optional<std::string> parse_json_string_token(const std::string& text, std::size_t& cursor) {
    skip_json_whitespace(text, cursor);
    if (cursor >= text.size() || text[cursor] != '"') {
        return std::nullopt;
    }

    ++cursor;
    std::string result;

    while (cursor < text.size()) {
        const char ch = text[cursor++];
        if (ch == '\\') {
            if (cursor >= text.size()) {
                return std::nullopt;
            }

            const char escaped = text[cursor++];
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
                    if (cursor + 4 > text.size()) {
                        return std::nullopt;
                    }
                    cursor += 4;
                    result.push_back('?');
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

std::vector<ConsultationMessage> build_consultation_history(const HttpRequest& request) {
    const std::string raw_history = trim_copy(pick_request_value(request, {"history", "history_messages", "historyMessages"}));
    std::vector<ConsultationMessage> messages;
    if (raw_history.empty()) {
        return messages;
    }

    std::size_t cursor = 0;
    skip_json_whitespace(raw_history, cursor);
    if (cursor >= raw_history.size() || raw_history[cursor] != '[') {
        return messages;
    }

    ++cursor;

    while (true) {
        skip_json_whitespace(raw_history, cursor);
        if (cursor >= raw_history.size()) {
            return std::vector<ConsultationMessage>{};
        }

        if (raw_history[cursor] == ']') {
            ++cursor;
            break;
        }

        if (raw_history[cursor] != '{') {
            return std::vector<ConsultationMessage>{};
        }
        ++cursor;

        ConsultationMessage message;
        while (true) {
            const auto key = parse_json_string_token(raw_history, cursor);
            if (!key.has_value()) {
                return std::vector<ConsultationMessage>{};
            }

            skip_json_whitespace(raw_history, cursor);
            if (cursor >= raw_history.size() || raw_history[cursor] != ':') {
                return std::vector<ConsultationMessage>{};
            }
            ++cursor;

            const auto value = parse_json_string_token(raw_history, cursor);
            if (!value.has_value()) {
                return std::vector<ConsultationMessage>{};
            }

            if (key.value() == "role") {
                message.role = value.value();
            } else if (key.value() == "content") {
                message.content = value.value();
            } else if (key.value() == "create_time" || key.value() == "createdAt") {
                message.create_time = value.value();
            }

            skip_json_whitespace(raw_history, cursor);
            if (cursor >= raw_history.size()) {
                return std::vector<ConsultationMessage>{};
            }

            if (raw_history[cursor] == '}') {
                ++cursor;
                break;
            }

            if (raw_history[cursor] != ',') {
                return std::vector<ConsultationMessage>{};
            }
            ++cursor;
        }

        if (!message.role.empty() && !message.content.empty()) {
            messages.push_back(message);
        }

        skip_json_whitespace(raw_history, cursor);
        if (cursor >= raw_history.size()) {
            return std::vector<ConsultationMessage>{};
        }

        if (raw_history[cursor] == ']') {
            ++cursor;
            break;
        }

        if (raw_history[cursor] != ',') {
            return std::vector<ConsultationMessage>{};
        }
        ++cursor;
    }

    return messages;
}

HttpResponse json_response(int status_code, const std::string& status_text, const std::string& body) {
    HttpResponse response;
    response.status_code = status_code;
    response.status_text = status_text;
    response.body = body;
    return response;
}

HttpResponse success_json(const std::string& data_json, int status_code = 200, const std::string& status_text = "OK") {
    return json_response(status_code, status_text, api_envelope_json(0, "ok", data_json));
}

HttpResponse error_json(int status_code, const std::string& status_text, const std::string& message) {
    return json_response(status_code, status_text, api_envelope_json(status_code, message, "null"));
}

HttpResponse method_not_allowed(const std::string& message) {
    return error_json(405, "Method Not Allowed", message);
}

HttpResponse unauthorized_json(const std::string& message) {
    return error_json(401, "Unauthorized", message);
}

HttpResponse forbidden_json(const std::string& message) {
    return error_json(403, "Forbidden", message);
}

std::string sse_event_payload(const std::string& event_name, const std::string& data_json) {
    return "event: " + event_name + "\n" + "data: " + data_json + "\n\n";
}

HttpResponse handle_request(
    const HttpRequest& request,
    bool mysql_enabled,
    AuthService& auth_service,
    CompetitionRepository& repository,
    ConsultationService& consultation_service,
    ActivityService& activity_service
) {
    if (request.method == "OPTIONS") {
        HttpResponse response;
        response.status_code = 204;
        response.status_text = "No Content";
        response.content_type = "text/plain; charset=utf-8";
        return response;
    }

    const std::string path = normalized_path(request.path);

    if (path == "/api/health") {
        if (request.method != "GET") {
            return method_not_allowed("Only GET is supported for /api/health.");
        }

        return success_json(
            std::string("{\"service\":\"competition-backend\",\"status\":\"up\",\"stage\":\"phase-1\",\"source\":")
            + json_string(mysql_enabled ? "mysql" : "local_storage")
            + "}"
        );
    }

    if (path == "/api/auth/login") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/auth/login is POST.");
        }

        std::string error_message;
        const auto login_result = auth_service.login(
            build_login_username(request),
            build_login_password(request),
            error_message
        );
        if (!login_result.has_value()) {
            return unauthorized_json(error_message);
        }

        return success_json(auth_login_result_to_json(login_result.value()));
    }

    if (path == "/api/auth/me") {
        if (request.method != "GET") {
            return method_not_allowed("Supported methods for /api/auth/me is GET.");
        }

        const auto user = request_user(request, auth_service);
        if (!user.has_value()) {
            return unauthorized_json("Please login first.");
        }

        return success_json("{\"user\":" + auth_user_to_json(user.value()) + "}");
    }

    if (path == "/api/auth/logout") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/auth/logout is POST.");
        }

        const std::string token = extract_bearer_token(request);
        if (token.empty()) {
            return unauthorized_json("Please login first.");
        }

        auth_service.logout(token);
        return success_json("{\"logged_out\":true}");
    }

    if (path == "/api/categories") {
        if (request.method == "GET") {
            return success_json(category_list_payload_to_json(repository.list_categories()));
        }

        if (request.method == "POST") {
            const auto user = request_user(request, auth_service);
            if (!user.has_value()) {
                return unauthorized_json("Please login first.");
            }
            if (user->role != "admin") {
                return forbidden_json("Only admin can create categories.");
            }

            std::string error_message;
            const auto category = repository.create_category(build_category_draft(request), error_message);
            if (!category.has_value()) {
                return error_json(400, "Bad Request", error_message);
            }
            return success_json(category_to_json(category.value()), 201, "Created");
        }

        return method_not_allowed("Supported methods for /api/categories are GET and POST.");
    }

    if (const auto category_id = extract_resource_id(path, "/api/categories/"); category_id.has_value()) {
        const auto user = request_user(request, auth_service);
        if (!user.has_value()) {
            return unauthorized_json("Please login first.");
        }
        if (user->role != "admin") {
            return forbidden_json("Only admin can manage categories.");
        }

        if (request.method == "PUT") {
            std::string error_message;
            const auto category = repository.update_category(category_id.value(), build_category_draft(request), error_message);
            if (!category.has_value()) {
                return error_json(error_message == "Category not found." ? 404 : 400, error_message == "Category not found." ? "Not Found" : "Bad Request", error_message);
            }
            return success_json(category_to_json(category.value()));
        }

        if (request.method == "DELETE") {
            std::string error_message;
            if (!repository.delete_category(category_id.value(), error_message)) {
                return error_json(404, "Not Found", error_message);
            }
            return success_json(delete_payload_to_json("category", category_id.value()));
        }

        return method_not_allowed("Supported methods for /api/categories/{id} are PUT and DELETE.");
    }

    if (path == "/api/competitions") {
        if (request.method == "GET") {
            const CompetitionFilter filter = build_filter(request);
            return success_json(competition_list_payload_to_json(repository.list(filter), filter));
        }

        if (request.method == "POST") {
            const auto user = request_user(request, auth_service);
            if (!user.has_value()) {
                return unauthorized_json("Please login first.");
            }
            if (user->role != "admin") {
                return forbidden_json("Only admin can create competitions.");
            }

            std::string error_message;
            const auto competition = repository.create_competition(build_competition_draft(request), error_message);
            if (!competition.has_value()) {
                return error_json(400, "Bad Request", error_message);
            }
            return success_json(competition_to_json(competition.value()), 201, "Created");
        }

        return method_not_allowed("Supported methods for /api/competitions are GET and POST.");
    }

    if (path == "/api/consultations/context") {
        if (request.method != "GET") {
            return method_not_allowed("Supported methods for /api/consultations/context is GET.");
        }

        const auto competition_id = build_competition_id_from_request(request);
        if (!competition_id.has_value()) {
            return error_json(400, "Bad Request", "competition_id 为必填项。");
        }

        std::string error_message;
        const auto context = consultation_service.build_context(repository, competition_id.value(), error_message);
        if (!context.has_value()) {
            const bool not_found = error_message == u8"未找到对应竞赛。";
            return error_json(not_found ? 404 : 400, not_found ? "Not Found" : "Bad Request", error_message);
        }

        return success_json(consultation_context_to_json(context.value()));
    }

    if (path == "/api/navigation/interpret") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/navigation/interpret is POST.");
        }

        std::string error_message;
        const auto intent = consultation_service.interpret_navigation(
            repository,
            build_navigation_command(request),
            error_message
        );
        if (!intent.has_value()) {
            return error_json(400, "Bad Request", error_message);
        }

        return success_json(navigation_intent_to_json(intent.value()));
    }

    if (path == "/api/records/searches") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/records/searches is POST.");
        }

        const auto user = request_user(request, auth_service);
        activity_service.log_search(
            user.has_value() ? std::optional<std::uint64_t>(user->user_id) : std::nullopt,
            pick_request_value(request, {"keyword", "q"}),
            pick_request_value(request, {"major"}),
            pick_request_value(request, {"competition_level", "level"}),
            pick_request_value(request, {"signup_status", "status"}),
            build_result_count(request),
            pick_request_value(request, {"trigger_source", "triggerSource", "source"})
        );

        return success_json("{\"logged\":true}");
    }

    if (path == "/api/records/navigation") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/records/navigation is POST.");
        }

        const auto user = request_user(request, auth_service);
        activity_service.log_navigation(
            user.has_value() ? std::optional<std::uint64_t>(user->user_id) : std::nullopt,
            repository,
            build_page_name(request, {"from_page", "fromPage"}),
            build_page_name(request, {"to_page", "toPage"}),
            build_page_name(request, {"action_name", "actionName", "action"}),
            build_competition_id_from_request(request).value_or(0)
        );

        return success_json("{\"logged\":true}");
    }

    if (path == "/api/records/dashboard") {
        if (request.method != "GET") {
            return method_not_allowed("Supported methods for /api/records/dashboard is GET.");
        }

        const ConsultationRecordsPayload consultations = consultation_service.build_records_payload(repository);
        const ActivityRecordsPayload activities = activity_service.build_records_payload();

        return success_json(
            "{"
            "\"consultations\":" + consultation_records_payload_to_json(consultations) + ","
            "\"activities\":" + activity_records_payload_to_json(activities) +
            "}"
        );
    }

    if (path == "/api/consultations/records") {
        if (request.method != "GET") {
            return method_not_allowed("Supported methods for /api/consultations/records is GET.");
        }

        return success_json(consultation_records_payload_to_json(consultation_service.build_records_payload(repository)));
    }

    if (path == "/api/consultations") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/consultations is POST.");
        }

        const auto competition_id = build_competition_id_from_request(request);
        if (!competition_id.has_value()) {
            return error_json(400, "Bad Request", "competition_id 为必填项。");
        }

        std::string error_message;
        const auto user = request_user(request, auth_service);
        const auto reply = consultation_service.ask(
            repository,
            user.has_value() ? std::optional<std::uint64_t>(user->user_id) : std::nullopt,
            competition_id.value(),
            build_consultation_session_id(request),
            build_consultation_history(request),
            build_consultation_question(request),
            build_consultation_mode(request),
            error_message
        );

        if (!reply.has_value()) {
            const bool not_found = error_message == u8"未找到对应竞赛。";
            return error_json(not_found ? 404 : 400, not_found ? "Not Found" : "Bad Request", error_message);
        }

        return success_json(consultation_reply_to_json(reply.value()));
    }

    if (path == "/api/consultations/stream") {
        if (request.method != "POST") {
            return method_not_allowed("Supported methods for /api/consultations/stream is POST.");
        }

        const auto competition_id = build_competition_id_from_request(request);
        if (!competition_id.has_value()) {
            return error_json(400, "Bad Request", "competition_id 为必填项。");
        }

        const auto user = request_user(request, auth_service);
        const std::optional<std::uint64_t> user_id = user.has_value()
            ? std::optional<std::uint64_t>(user->user_id)
            : std::nullopt;
        const std::string session_id = build_consultation_session_id(request);
        const std::vector<ConsultationMessage> history_messages = build_consultation_history(request);
        const std::string question = build_consultation_question(request);

        HttpResponse response;
        response.content_type = "text/event-stream; charset=utf-8";
        response.streaming = true;
        response.headers.push_back({"Cache-Control", "no-cache"});
        response.headers.push_back({"X-Accel-Buffering", "no"});
        response.headers.push_back({"Access-Control-Expose-Headers", "Content-Type"});
        response.stream_handler = [
            &repository,
            &consultation_service,
            user_id,
            competition_id_value = competition_id.value(),
            session_id,
            history_messages,
            question
        ](const HttpStreamWriter& writer) {
            const auto emit = [&](const std::string& event_name, const std::string& data_json) {
                return writer(sse_event_payload(event_name, data_json));
            };

            std::string error_message;
            const auto reply = consultation_service.ask_stream(
                repository,
                user_id,
                competition_id_value,
                session_id,
                history_messages,
                question,
                emit,
                error_message
            );

            if (!reply.has_value() && error_message != u8"流式连接已断开。") {
                emit("error", "{\"message\":" + json_string(error_message.empty() ? std::string(u8"咨询失败。") : error_message) + "}");
            }

            return true;
        };
        return response;
    }

    if (const auto competition_id = extract_resource_id(path, "/api/competitions/"); competition_id.has_value()) {
        if (request.method == "GET") {
            const auto competition = repository.find_by_id(competition_id.value(), true);
            if (!competition.has_value()) {
                return error_json(404, "Not Found", "Competition not found.");
            }
            return success_json(competition_to_json(competition.value()));
        }

        const auto user = request_user(request, auth_service);
        if (!user.has_value()) {
            return unauthorized_json("Please login first.");
        }
        if (user->role != "admin") {
            return forbidden_json("Only admin can manage competitions.");
        }

        if (request.method == "PUT") {
            std::string error_message;
            const auto competition = repository.update_competition(competition_id.value(), build_competition_draft(request), error_message);
            if (!competition.has_value()) {
                return error_json(error_message == "Competition not found." ? 404 : 400, error_message == "Competition not found." ? "Not Found" : "Bad Request", error_message);
            }
            return success_json(competition_to_json(competition.value()));
        }

        if (request.method == "DELETE") {
            std::string error_message;
            if (!repository.delete_competition(competition_id.value(), error_message)) {
                return error_json(404, "Not Found", error_message);
            }
            return success_json(delete_payload_to_json("competition", competition_id.value()));
        }

        return method_not_allowed("Supported methods for /api/competitions/{id} are GET, PUT and DELETE.");
    }

    return error_json(404, "Not Found", "Route not found.");
}

}  // namespace

int main(int argc, char* argv[]) {
    unsigned short port = 8080;

    if (argc > 1) {
        try {
            const int parsed_port = std::stoi(argv[1]);
            if (parsed_port <= 0 || parsed_port > 65535) {
                std::cerr << "Port must be between 1 and 65535." << std::endl;
                return 1;
            }
            port = static_cast<unsigned short>(parsed_port);
        } catch (...) {
            std::cerr << "Invalid port argument." << std::endl;
            return 1;
        }
    }

    MySqlClient database_client;
    if (database_client.available()) {
        std::cout << "MySQL connected. Database: " << database_client.database_name() << std::endl;
    } else if (!database_client.last_error().empty()) {
        std::cout << "MySQL unavailable, falling back to local storage. Reason: "
                  << database_client.last_error() << std::endl;
    } else {
        std::cout << "MySQL unavailable, falling back to local storage." << std::endl;
    }

    AuthService auth_service(&database_client);
    CompetitionRepository repository(&database_client);
    ConsultationService consultation_service(&database_client);
    ActivityService activity_service(&database_client);
    HttpServer server("127.0.0.1", port);
    std::string error_message;

    if (!server.start(error_message)) {
        std::cerr << "Failed to start backend server: " << error_message << std::endl;
        return 1;
    }

    server.serve([&database_client, &auth_service, &repository, &consultation_service, &activity_service](const HttpRequest& request) {
        return handle_request(request, database_client.available(), auth_service, repository, consultation_service, activity_service);
    });

    return 0;
}

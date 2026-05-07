#include "competition_repository.h"
#include "http_server.h"
#include "json_utils.h"

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

HttpResponse handle_request(const HttpRequest& request, CompetitionRepository& repository) {
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

        return success_json("{\"service\":\"competition-backend\",\"status\":\"up\",\"stage\":\"admin-crud\",\"source\":\"demo_repository\"}");
    }

    if (path == "/api/categories") {
        if (request.method == "GET") {
            return success_json(category_list_payload_to_json(repository.list_categories()));
        }

        if (request.method == "POST") {
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
            std::string error_message;
            const auto competition = repository.create_competition(build_competition_draft(request), error_message);
            if (!competition.has_value()) {
                return error_json(400, "Bad Request", error_message);
            }
            return success_json(competition_to_json(competition.value()), 201, "Created");
        }

        return method_not_allowed("Supported methods for /api/competitions are GET and POST.");
    }

    if (const auto competition_id = extract_resource_id(path, "/api/competitions/"); competition_id.has_value()) {
        if (request.method == "GET") {
            const auto competition = repository.find_by_id(competition_id.value(), true);
            if (!competition.has_value()) {
                return error_json(404, "Not Found", "Competition not found.");
            }
            return success_json(competition_to_json(competition.value()));
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

    CompetitionRepository repository;
    HttpServer server("127.0.0.1", port);
    std::string error_message;

    if (!server.start(error_message)) {
        std::cerr << "Failed to start backend server: " << error_message << std::endl;
        return 1;
    }

    server.serve([&repository](const HttpRequest& request) {
        return handle_request(request, repository);
    });

    return 0;
}

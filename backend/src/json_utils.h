#pragma once

#include "competition_repository.h"

#include <sstream>
#include <string>
#include <vector>

inline std::string escape_json(const std::string& value) {
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

inline std::string json_string(const std::string& value) {
    return "\"" + escape_json(value) + "\"";
}

inline std::string category_to_json(const Category& category) {
    std::ostringstream json;
    json
        << "{"
        << "\"category_id\":" << category.category_id << ","
        << "\"category_name\":" << json_string(category.category_name) << ","
        << "\"category_desc\":" << json_string(category.category_desc) << ","
        << "\"sort_no\":" << category.sort_no << ","
        << "\"competition_count\":" << category.competition_count << ","
        << "\"update_time\":" << json_string(category.update_time)
        << "}";
    return json.str();
}

inline std::string competition_to_json(const Competition& competition) {
    std::ostringstream json;
    json
        << "{"
        << "\"competition_id\":" << competition.competition_id << ","
        << "\"category_id\":" << competition.category_id << ","
        << "\"category_name\":" << json_string(competition.category_name) << ","
        << "\"competition_name\":" << json_string(competition.competition_name) << ","
        << "\"organizer\":" << json_string(competition.organizer) << ","
        << "\"competition_level\":" << json_string(competition.competition_level) << ","
        << "\"signup_status\":" << json_string(competition.signup_status) << ","
        << "\"suitable_major\":" << json_string(competition.suitable_major) << ","
        << "\"suitable_grade\":" << json_string(competition.suitable_grade) << ","
        << "\"signup_start\":" << json_string(competition.signup_start) << ","
        << "\"signup_end\":" << json_string(competition.signup_end) << ","
        << "\"competition_start\":" << json_string(competition.competition_start) << ","
        << "\"competition_end\":" << json_string(competition.competition_end) << ","
        << "\"official_url\":" << json_string(competition.official_url) << ","
        << "\"competition_desc\":" << json_string(competition.competition_desc) << ","
        << "\"participation_rules\":" << json_string(competition.participation_rules) << ","
        << "\"display_status\":" << (competition.display_status ? "true" : "false") << ","
        << "\"create_time\":" << json_string(competition.create_time) << ","
        << "\"update_time\":" << json_string(competition.update_time)
        << "}";
    return json.str();
}

inline std::string filter_to_json(const CompetitionFilter& filter) {
    std::ostringstream json;
    json
        << "{"
        << "\"keyword\":" << json_string(filter.keyword) << ","
        << "\"major\":" << json_string(filter.major) << ","
        << "\"competition_level\":" << json_string(filter.competition_level) << ","
        << "\"signup_status\":" << json_string(filter.signup_status) << ","
        << "\"suitable_grade\":" << json_string(filter.suitable_grade) << ","
        << "\"category_name\":" << json_string(filter.category_name) << ","
        << "\"include_hidden\":" << (filter.include_hidden ? "true" : "false") << ","
        << "\"category_id\":";

    if (filter.category_id.has_value()) {
        json << filter.category_id.value();
    } else {
        json << "null";
    }

    json << "}";
    return json.str();
}

inline std::string competition_list_payload_to_json(const std::vector<Competition>& competitions, const CompetitionFilter& filter) {
    std::ostringstream json;
    json << "{";
    json << "\"items\":[";

    for (std::size_t index = 0; index < competitions.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << competition_to_json(competitions[index]);
    }

    json
        << "],"
        << "\"total\":" << competitions.size() << ","
        << "\"filters\":" << filter_to_json(filter) << ","
        << "\"source\":\"demo_repository\""
        << "}";
    return json.str();
}

inline std::string category_list_payload_to_json(const std::vector<Category>& categories) {
    std::ostringstream json;
    json << "{";
    json << "\"items\":[";

    for (std::size_t index = 0; index < categories.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << category_to_json(categories[index]);
    }

    json
        << "],"
        << "\"total\":" << categories.size()
        << "}";
    return json.str();
}

inline std::string delete_payload_to_json(const std::string& entity_name, std::uint64_t entity_id) {
    std::ostringstream json;
    json
        << "{"
        << "\"deleted\":true,"
        << "\"entity\":" << json_string(entity_name) << ","
        << "\"id\":" << entity_id
        << "}";
    return json.str();
}

inline std::string api_envelope_json(int code, const std::string& message, const std::string& data_json) {
    std::ostringstream json;
    json
        << "{"
        << "\"code\":" << code << ","
        << "\"message\":" << json_string(message) << ","
        << "\"data\":" << data_json
        << "}";
    return json.str();
}

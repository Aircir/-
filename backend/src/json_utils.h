#pragma once

#include "activity_service.h"
#include "auth_service.h"
#include "consultation_service.h"
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

inline std::string auth_user_to_json(const AuthUser& user) {
    std::ostringstream json;
    json
        << "{"
        << "\"user_id\":" << user.user_id << ","
        << "\"username\":" << json_string(user.username) << ","
        << "\"display_name\":" << json_string(user.display_name) << ","
        << "\"role\":" << json_string(user.role)
        << "}";
    return json.str();
}

inline std::string auth_login_result_to_json(const AuthLoginResult& result) {
    std::ostringstream json;
    json
        << "{"
        << "\"token\":" << json_string(result.token) << ","
        << "\"user\":" << auth_user_to_json(result.user) << ","
        << "\"login_time\":" << json_string(result.login_time)
        << "}";
    return json.str();
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
        << "\"source\":\"competition_repository\""
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

inline std::string string_list_to_json(const std::vector<std::string>& items) {
    std::ostringstream json;
    json << "[";

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << json_string(items[index]);
    }

    json << "]";
    return json.str();
}

inline std::string consultation_message_to_json(const ConsultationMessage& message) {
    std::ostringstream json;
    json
        << "{"
        << "\"role\":" << json_string(message.role) << ","
        << "\"content\":" << json_string(message.content) << ","
        << "\"create_time\":" << json_string(message.create_time)
        << "}";
    return json.str();
}

inline std::string consultation_message_list_to_json(const std::vector<ConsultationMessage>& messages) {
    std::ostringstream json;
    json << "[";

    for (std::size_t index = 0; index < messages.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << consultation_message_to_json(messages[index]);
    }

    json << "]";
    return json.str();
}

inline std::string consultation_context_to_json(const ConsultationContext& context) {
    std::ostringstream json;
    json
        << "{"
        << "\"competition\":" << competition_to_json(context.competition) << ","
        << "\"welcome_message\":" << json_string(context.welcome_message) << ","
        << "\"example_questions\":" << string_list_to_json(context.example_questions)
        << "}";
    return json.str();
}

inline std::string consultation_reply_to_json(const ConsultationReply& reply) {
    std::ostringstream json;
    json
        << "{"
        << "\"session_id\":" << json_string(reply.session_id) << ","
        << "\"competition\":" << competition_to_json(reply.competition) << ","
        << "\"question\":" << json_string(reply.question) << ","
        << "\"answer\":" << json_string(reply.answer) << ","
        << "\"response_mode\":" << json_string(reply.response_mode) << ","
        << "\"follow_up_questions\":" << string_list_to_json(reply.follow_up_questions) << ","
        << "\"messages\":" << consultation_message_list_to_json(reply.messages) << ","
        << "\"update_time\":" << json_string(reply.update_time)
        << "}";
    return json.str();
}

inline std::string navigation_intent_to_json(const NavigationIntent& intent) {
    std::ostringstream json;
    json
        << "{"
        << "\"action\":" << json_string(intent.action) << ","
        << "\"target_page\":" << json_string(intent.target_page) << ","
        << "\"competition_id\":" << intent.competition_id << ","
        << "\"competition_name\":" << json_string(intent.competition_name) << ","
        << "\"keyword\":" << json_string(intent.keyword) << ","
        << "\"major\":" << json_string(intent.major) << ","
        << "\"competition_level\":" << json_string(intent.competition_level) << ","
        << "\"signup_status\":" << json_string(intent.signup_status) << ","
        << "\"category_name\":" << json_string(intent.category_name) << ","
        << "\"feedback_message\":" << json_string(intent.feedback_message) << ","
        << "\"used_ai\":" << (intent.used_ai ? "true" : "false")
        << "}";
    return json.str();
}

inline std::string consultation_record_item_to_json(const ConsultationRecordItem& item) {
    std::ostringstream json;
    json
        << "{"
        << "\"session_id\":" << json_string(item.session_id) << ","
        << "\"competition_id\":" << item.competition_id << ","
        << "\"competition_name\":" << json_string(item.competition_name) << ","
        << "\"category_name\":" << json_string(item.category_name) << ","
        << "\"latest_question\":" << json_string(item.latest_question) << ","
        << "\"latest_answer\":" << json_string(item.latest_answer) << ","
        << "\"message_count\":" << item.message_count << ","
        << "\"update_time\":" << json_string(item.update_time)
        << "}";
    return json.str();
}

inline std::string consultation_record_item_list_to_json(const std::vector<ConsultationRecordItem>& items) {
    std::ostringstream json;
    json << "[";

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << consultation_record_item_to_json(items[index]);
    }

    json << "]";
    return json.str();
}

inline std::string consultation_category_stat_to_json(const ConsultationCategoryStat& item) {
    std::ostringstream json;
    json
        << "{"
        << "\"category_name\":" << json_string(item.category_name) << ","
        << "\"session_count\":" << item.session_count << ","
        << "\"message_count\":" << item.message_count
        << "}";
    return json.str();
}

inline std::string consultation_category_stat_list_to_json(const std::vector<ConsultationCategoryStat>& items) {
    std::ostringstream json;
    json << "[";

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << consultation_category_stat_to_json(items[index]);
    }

    json << "]";
    return json.str();
}

inline std::string consultation_overview_to_json(const ConsultationOverview& overview) {
    std::ostringstream json;
    json
        << "{"
        << "\"total_sessions\":" << overview.total_sessions << ","
        << "\"total_messages\":" << overview.total_messages << ","
        << "\"competition_total\":" << overview.competition_total << ","
        << "\"hot_category_total\":" << overview.hot_category_total
        << "}";
    return json.str();
}

inline std::string consultation_records_payload_to_json(const ConsultationRecordsPayload& payload) {
    std::ostringstream json;
    json
        << "{"
        << "\"overview\":" << consultation_overview_to_json(payload.overview) << ","
        << "\"items\":" << consultation_record_item_list_to_json(payload.items) << ","
        << "\"hot_categories\":" << consultation_category_stat_list_to_json(payload.hot_categories)
        << "}";
    return json.str();
}

inline std::string search_record_item_to_json(const SearchRecordItem& item) {
    std::ostringstream json;
    json
        << "{"
        << "\"search_id\":" << item.search_id << ","
        << "\"keyword\":" << json_string(item.keyword) << ","
        << "\"major\":" << json_string(item.major) << ","
        << "\"competition_level\":" << json_string(item.competition_level) << ","
        << "\"signup_status\":" << json_string(item.signup_status) << ","
        << "\"result_count\":" << item.result_count << ","
        << "\"trigger_source\":" << json_string(item.trigger_source) << ","
        << "\"create_time\":" << json_string(item.create_time)
        << "}";
    return json.str();
}

inline std::string search_record_list_to_json(const std::vector<SearchRecordItem>& items) {
    std::ostringstream json;
    json << "[";

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << search_record_item_to_json(items[index]);
    }

    json << "]";
    return json.str();
}

inline std::string navigation_record_item_to_json(const NavigationRecordItem& item) {
    std::ostringstream json;
    json
        << "{"
        << "\"navigation_id\":" << item.navigation_id << ","
        << "\"from_page\":" << json_string(item.from_page) << ","
        << "\"to_page\":" << json_string(item.to_page) << ","
        << "\"action_name\":" << json_string(item.action_name) << ","
        << "\"competition_id\":" << item.competition_id << ","
        << "\"competition_name\":" << json_string(item.competition_name) << ","
        << "\"create_time\":" << json_string(item.create_time)
        << "}";
    return json.str();
}

inline std::string navigation_record_list_to_json(const std::vector<NavigationRecordItem>& items) {
    std::ostringstream json;
    json << "[";

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            json << ",";
        }
        json << navigation_record_item_to_json(items[index]);
    }

    json << "]";
    return json.str();
}

inline std::string activity_overview_to_json(const ActivityOverview& overview) {
    std::ostringstream json;
    json
        << "{"
        << "\"total_searches\":" << overview.total_searches << ","
        << "\"total_navigations\":" << overview.total_navigations
        << "}";
    return json.str();
}

inline std::string activity_records_payload_to_json(const ActivityRecordsPayload& payload) {
    std::ostringstream json;
    json
        << "{"
        << "\"overview\":" << activity_overview_to_json(payload.overview) << ","
        << "\"search_records\":" << search_record_list_to_json(payload.search_records) << ","
        << "\"navigation_records\":" << navigation_record_list_to_json(payload.navigation_records)
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

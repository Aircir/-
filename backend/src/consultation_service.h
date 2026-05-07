#pragma once

#include "competition_repository.h"
#include "mysql_client.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ConsultationMessage {
    std::string role;
    std::string content;
    std::string create_time;
};

struct ConsultationSession {
    std::string session_id;
    std::uint64_t competition_id = 0;
    std::vector<ConsultationMessage> messages;
    std::string update_time;
};

struct ConsultationContext {
    Competition competition;
    std::string welcome_message;
    std::vector<std::string> example_questions;
};

struct ConsultationReply {
    std::string session_id;
    Competition competition;
    std::string question;
    std::string answer;
    std::string response_mode;
    std::vector<std::string> follow_up_questions;
    std::vector<ConsultationMessage> messages;
    std::string update_time;
};

struct ConsultationRecordItem {
    std::string session_id;
    std::uint64_t competition_id = 0;
    std::string competition_name;
    std::string category_name;
    std::string latest_question;
    std::string latest_answer;
    std::size_t message_count = 0;
    std::string update_time;
};

struct ConsultationCategoryStat {
    std::string category_name;
    std::size_t session_count = 0;
    std::size_t message_count = 0;
};

struct ConsultationOverview {
    std::size_t total_sessions = 0;
    std::size_t total_messages = 0;
    std::size_t competition_total = 0;
    std::size_t hot_category_total = 0;
};

struct ConsultationRecordsPayload {
    ConsultationOverview overview;
    std::vector<ConsultationRecordItem> items;
    std::vector<ConsultationCategoryStat> hot_categories;
};

struct NavigationIntent {
    std::string action;
    std::string target_page;
    std::uint64_t competition_id = 0;
    std::string competition_name;
    std::string keyword;
    std::string major;
    std::string competition_level;
    std::string signup_status;
    std::string category_name;
    std::string feedback_message;
    bool used_ai = false;
};

class ConsultationService {
public:
    explicit ConsultationService(MySqlClient* database_client = nullptr);

    std::optional<ConsultationContext> build_context(
        CompetitionRepository& repository,
        std::uint64_t competition_id,
        std::string& error_message
    ) const;

    std::optional<ConsultationReply> ask(
        CompetitionRepository& repository,
        std::optional<std::uint64_t> user_id,
        std::uint64_t competition_id,
        const std::string& session_id,
        const std::vector<ConsultationMessage>& history_messages,
        const std::string& question,
        const std::string& response_mode,
        std::string& error_message
    );

    std::optional<ConsultationReply> ask_stream(
        CompetitionRepository& repository,
        std::optional<std::uint64_t> user_id,
        std::uint64_t competition_id,
        const std::string& session_id,
        const std::vector<ConsultationMessage>& history_messages,
        const std::string& question,
        const std::function<bool(const std::string&, const std::string&)>& emit_event,
        std::string& error_message
    );

    std::optional<NavigationIntent> interpret_navigation(
        CompetitionRepository& repository,
        const std::string& command,
        std::string& error_message
    ) const;

    ConsultationRecordsPayload build_records_payload(CompetitionRepository& repository) const;

private:
    MySqlClient* database_client_ = nullptr;
    bool using_mysql_ = false;
    std::unordered_map<std::string, ConsultationSession> sessions_;
    std::uint64_t next_session_id_ = 1;
    std::string sessions_file_path_;
    std::string messages_file_path_;
    std::string api_key_;
    std::string responses_url_;
    std::string chat_completions_url_;
    std::string api_format_;
    std::string model_name_;
    int timeout_seconds_ = 60;

    static std::string current_timestamp();
    static std::string trim_copy(std::string value);
    static std::string to_lower(std::string value);
    static std::string normalize_mode(std::string mode);
    static std::string normalize_session_id(std::string session_id);
    static std::string fallback_text(const std::string& value, const std::string& fallback);

    static std::string read_env_value(const char* name);
    static std::string resolve_responses_url();
    static std::string resolve_chat_completions_url();
    static std::string resolve_api_format();
    static int resolve_timeout_seconds();
    static std::vector<ConsultationMessage> normalize_history_messages(const std::vector<ConsultationMessage>& history_messages);

    bool ensure_mysql_schema();
    bool load_sessions_from_mysql();
    bool persist_consultation_record(
        std::optional<std::uint64_t> user_id,
        const Competition& competition,
        const ConsultationReply& reply
    ) const;
    bool load_persisted_state();
    bool persist() const;
    std::string next_session_id_text();
    std::vector<std::string> build_example_questions(const Competition& competition) const;
    std::string build_welcome_message(const Competition& competition) const;
    std::vector<std::string> build_follow_up_questions(const Competition& competition) const;
    std::string build_competition_brief(const Competition& competition) const;
    std::string build_system_instructions(const Competition& competition) const;
    std::string build_openai_responses_request_json(const Competition& competition, const std::vector<ConsultationMessage>& messages) const;
    std::string build_openai_chat_request_json(const Competition& competition, const std::vector<ConsultationMessage>& messages, bool stream = false) const;
    std::string build_navigation_catalog(CompetitionRepository& repository) const;
    std::string build_navigation_system_instructions(CompetitionRepository& repository) const;
    std::string build_navigation_request_json(CompetitionRepository& repository, const std::string& command) const;
    NavigationIntent build_navigation_intent_by_heuristic(const std::string& command) const;
    void resolve_navigation_intent(CompetitionRepository& repository, NavigationIntent& intent) const;
    std::optional<std::string> request_openai_answer(
        const Competition& competition,
        const std::vector<ConsultationMessage>& messages,
        std::string& error_message
    ) const;
    std::optional<std::string> request_openai_answer_stream(
        const Competition& competition,
        const std::vector<ConsultationMessage>& messages,
        const std::function<bool(const std::string&)>& on_delta,
        std::string& error_message
    ) const;
};

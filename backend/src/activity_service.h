#pragma once

#include "competition_repository.h"
#include "mysql_client.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct SearchRecordItem {
    std::uint64_t search_id = 0;
    std::string keyword;
    std::string major;
    std::string competition_level;
    std::string signup_status;
    std::size_t result_count = 0;
    std::string trigger_source;
    std::string create_time;
};

struct NavigationRecordItem {
    std::uint64_t navigation_id = 0;
    std::string from_page;
    std::string to_page;
    std::string action_name;
    std::uint64_t competition_id = 0;
    std::string competition_name;
    std::string create_time;
};

struct ActivityOverview {
    std::size_t total_searches = 0;
    std::size_t total_navigations = 0;
};

struct ActivityRecordsPayload {
    ActivityOverview overview;
    std::vector<SearchRecordItem> search_records;
    std::vector<NavigationRecordItem> navigation_records;
};

class ActivityService {
public:
    explicit ActivityService(MySqlClient* database_client = nullptr);

    void log_search(
        std::optional<std::uint64_t> user_id,
        const std::string& keyword,
        const std::string& major,
        const std::string& competition_level,
        const std::string& signup_status,
        std::size_t result_count,
        const std::string& trigger_source
    );

    void log_navigation(
        std::optional<std::uint64_t> user_id,
        CompetitionRepository& repository,
        const std::string& from_page,
        const std::string& to_page,
        const std::string& action_name,
        std::uint64_t competition_id
    );

    ActivityRecordsPayload build_records_payload() const;

private:
    MySqlClient* database_client_ = nullptr;
    bool using_mysql_ = false;
    std::vector<SearchRecordItem> search_records_;
    std::vector<NavigationRecordItem> navigation_records_;
    std::uint64_t next_search_id_ = 1;
    std::uint64_t next_navigation_id_ = 1;
    std::string search_records_file_path_;
    std::string navigation_records_file_path_;

    static std::string current_timestamp();
    static std::string trim_copy(std::string value);
    static std::string normalize_page_name(std::string value);
    static std::string fallback_text(const std::string& value, const std::string& fallback);
    bool ensure_mysql_schema();
    bool load_from_mysql();
    bool load_persisted_state();
    bool persist() const;
};

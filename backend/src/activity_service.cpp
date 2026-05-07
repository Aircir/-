#include "activity_service.h"
#include "persistence_utils.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

template <typename Item>
std::vector<Item> take_recent_items(const std::vector<Item>& source, std::size_t limit) {
    std::vector<Item> result;
    if (source.empty() || limit == 0) {
        return result;
    }

    const std::size_t start = source.size() > limit ? source.size() - limit : 0;
    result.reserve(source.size() - start);
    for (std::size_t index = source.size(); index > start; --index) {
        result.push_back(source[index - 1]);
    }

    return result;
}

std::uint64_t parse_u64_or_zero(const std::string& value) {
    const auto parsed = persistence::parse_u64(value);
    return parsed.has_value() ? parsed.value() : 0;
}

bool mysql_column_exists(MySqlClient* database_client, const std::string& table_name, const std::string& column_name) {
    if (database_client == nullptr || !database_client->available()) {
        return false;
    }

    const auto result = database_client->query(
        "SHOW COLUMNS FROM `" + table_name + "` LIKE " + database_client->quote(column_name)
    );
    return result.has_value() && !result->rows.empty();
}

}  // namespace

ActivityService::ActivityService(MySqlClient* database_client)
    : database_client_(database_client),
      using_mysql_(database_client_ != nullptr && database_client_->available()),
      search_records_file_path_(persistence::data_file_path("search_records.tsv")),
      navigation_records_file_path_(persistence::data_file_path("navigation_records.tsv")) {
    if (using_mysql_) {
        if (ensure_mysql_schema() && load_from_mysql()) {
            return;
        }
        using_mysql_ = false;
    }

    load_persisted_state();
}

void ActivityService::log_search(
    std::optional<std::uint64_t> user_id,
    const std::string& keyword,
    const std::string& major,
    const std::string& competition_level,
    const std::string& signup_status,
    std::size_t result_count,
    const std::string& trigger_source
) {
    if (trim_copy(keyword).empty()
        && trim_copy(major).empty()
        && trim_copy(competition_level).empty()
        && trim_copy(signup_status).empty()
        && trim_copy(trigger_source).empty()) {
        return;
    }

    SearchRecordItem record;
    record.search_id = next_search_id_++;
    record.keyword = trim_copy(keyword);
    record.major = trim_copy(major);
    record.competition_level = trim_copy(competition_level);
    record.signup_status = trim_copy(signup_status);
    record.result_count = result_count;
    record.trigger_source = trim_copy(trigger_source);
    record.create_time = current_timestamp();

    if (using_mysql_) {
        const std::string user_id_sql = user_id.has_value() ? std::to_string(user_id.value()) : "NULL";
        if (database_client_->execute(
            "INSERT INTO search_log (user_id, keyword, major, competition_level, signup_status, trigger_source, result_count, create_time) VALUES ("
            + user_id_sql + ","
            + database_client_->nullable_quote(record.keyword) + ","
            + database_client_->nullable_quote(record.major) + ","
            + database_client_->nullable_quote(record.competition_level) + ","
            + database_client_->nullable_quote(record.signup_status) + ","
            + database_client_->nullable_quote(record.trigger_source) + ","
            + std::to_string(record.result_count) + ","
            + database_client_->quote(record.create_time) + ")"
        )) {
            record.search_id = database_client_->last_insert_id();
        }
    }

    search_records_.push_back(record);
    if (!using_mysql_) {
        persist();
    }
}

void ActivityService::log_navigation(
    std::optional<std::uint64_t> user_id,
    CompetitionRepository& repository,
    const std::string& from_page,
    const std::string& to_page,
    const std::string& action_name,
    std::uint64_t competition_id
) {
    const std::string normalized_from = normalize_page_name(from_page);
    const std::string normalized_to = normalize_page_name(to_page);
    const std::string normalized_action = trim_copy(action_name);

    if (normalized_from.empty() && normalized_to.empty() && normalized_action.empty() && competition_id == 0) {
        return;
    }

    NavigationRecordItem record;
    record.navigation_id = next_navigation_id_++;
    record.from_page = normalized_from;
    record.to_page = normalized_to;
    record.action_name = normalized_action;
    record.competition_id = competition_id;
    record.create_time = current_timestamp();

    if (competition_id > 0) {
        const auto competition = repository.find_by_id(competition_id, true);
        record.competition_name = competition.has_value()
            ? fallback_text(competition->competition_name, "")
            : "";
    }

    if (using_mysql_) {
        const std::string user_id_sql = user_id.has_value() ? std::to_string(user_id.value()) : "NULL";
        if (database_client_->execute(
            "INSERT INTO navigation_command (user_id, from_page, to_page, action_name, competition_id, competition_name, create_time) VALUES ("
            + user_id_sql + ","
            + database_client_->nullable_quote(record.from_page) + ","
            + database_client_->nullable_quote(record.to_page) + ","
            + database_client_->nullable_quote(record.action_name) + ","
            + (competition_id == 0 ? std::string("NULL") : std::to_string(competition_id)) + ","
            + database_client_->nullable_quote(record.competition_name) + ","
            + database_client_->quote(record.create_time) + ")"
        )) {
            record.navigation_id = database_client_->last_insert_id();
        }
    }

    navigation_records_.push_back(record);
    if (!using_mysql_) {
        persist();
    }
}

ActivityRecordsPayload ActivityService::build_records_payload() const {
    ActivityRecordsPayload payload;
    payload.overview.total_searches = search_records_.size();
    payload.overview.total_navigations = navigation_records_.size();
    payload.search_records = take_recent_items(search_records_, 20);
    payload.navigation_records = take_recent_items(navigation_records_, 20);
    return payload;
}

std::string ActivityService::current_timestamp() {
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

std::string ActivityService::trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string ActivityService::normalize_page_name(std::string value) {
    value = trim_copy(std::move(value));
    if (value.empty()) {
        return "";
    }

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (value.size() > 5 && value.substr(value.size() - 5) == ".html") {
        value.resize(value.size() - 5);
    }

    return value;
}

std::string ActivityService::fallback_text(const std::string& value, const std::string& fallback) {
    return trim_copy(value).empty() ? fallback : value;
}

bool ActivityService::load_persisted_state() {
    if (using_mysql_) {
        return load_from_mysql();
    }

    search_records_.clear();
    navigation_records_.clear();

    std::vector<std::string> fields;
    std::uint64_t max_search_id = 0;
    std::uint64_t max_navigation_id = 0;

    if (persistence::file_exists(search_records_file_path_)) {
        std::vector<std::string> search_lines;
        if (!persistence::read_lines(search_records_file_path_, search_lines)) {
            next_search_id_ = 1;
            next_navigation_id_ = 1;
            return false;
        }

        for (const std::string& line : search_lines) {
            if (line.empty()) {
                continue;
            }

            if (!persistence::split_fields(line, fields) || fields.size() != 8) {
                search_records_.clear();
                navigation_records_.clear();
                next_search_id_ = 1;
                next_navigation_id_ = 1;
                return false;
            }

            const auto search_id = persistence::parse_u64(fields[0]);
            const auto result_count = persistence::parse_size(fields[5]);
            if (!search_id.has_value() || !result_count.has_value()) {
                search_records_.clear();
                navigation_records_.clear();
                next_search_id_ = 1;
                next_navigation_id_ = 1;
                return false;
            }

            SearchRecordItem item;
            item.search_id = search_id.value();
            item.keyword = fields[1];
            item.major = fields[2];
            item.competition_level = fields[3];
            item.signup_status = fields[4];
            item.result_count = result_count.value();
            item.trigger_source = fields[6];
            item.create_time = fields[7];
            search_records_.push_back(item);
            max_search_id = std::max(max_search_id, item.search_id);
        }
    }

    if (persistence::file_exists(navigation_records_file_path_)) {
        std::vector<std::string> navigation_lines;
        if (!persistence::read_lines(navigation_records_file_path_, navigation_lines)) {
            search_records_.clear();
            navigation_records_.clear();
            next_search_id_ = 1;
            next_navigation_id_ = 1;
            return false;
        }

        for (const std::string& line : navigation_lines) {
            if (line.empty()) {
                continue;
            }

            if (!persistence::split_fields(line, fields) || fields.size() != 7) {
                search_records_.clear();
                navigation_records_.clear();
                next_search_id_ = 1;
                next_navigation_id_ = 1;
                return false;
            }

            const auto navigation_id = persistence::parse_u64(fields[0]);
            const auto competition_id = persistence::parse_u64(fields[4]);
            if (!navigation_id.has_value() || !competition_id.has_value()) {
                search_records_.clear();
                navigation_records_.clear();
                next_search_id_ = 1;
                next_navigation_id_ = 1;
                return false;
            }

            NavigationRecordItem item;
            item.navigation_id = navigation_id.value();
            item.from_page = fields[1];
            item.to_page = fields[2];
            item.action_name = fields[3];
            item.competition_id = competition_id.value();
            item.competition_name = fields[5];
            item.create_time = fields[6];
            navigation_records_.push_back(item);
            max_navigation_id = std::max(max_navigation_id, item.navigation_id);
        }
    }

    next_search_id_ = max_search_id + 1;
    next_navigation_id_ = max_navigation_id + 1;
    if (next_search_id_ == 0) {
        next_search_id_ = 1;
    }
    if (next_navigation_id_ == 0) {
        next_navigation_id_ = 1;
    }
    return true;
}

bool ActivityService::persist() const {
    if (using_mysql_) {
        return true;
    }

    std::vector<std::string> search_lines;
    std::vector<std::string> navigation_lines;

    search_lines.reserve(search_records_.size());
    for (const SearchRecordItem& item : search_records_) {
        search_lines.push_back(persistence::join_fields({
            std::to_string(item.search_id),
            item.keyword,
            item.major,
            item.competition_level,
            item.signup_status,
            std::to_string(item.result_count),
            item.trigger_source,
            item.create_time
        }));
    }

    navigation_lines.reserve(navigation_records_.size());
    for (const NavigationRecordItem& item : navigation_records_) {
        navigation_lines.push_back(persistence::join_fields({
            std::to_string(item.navigation_id),
            item.from_page,
            item.to_page,
            item.action_name,
            std::to_string(item.competition_id),
            item.competition_name,
            item.create_time
        }));
    }

    return persistence::write_lines(search_records_file_path_, search_lines)
        && persistence::write_lines(navigation_records_file_path_, navigation_lines);
}

bool ActivityService::ensure_mysql_schema() {
    const bool schema_ready = database_client_->execute(
        "CREATE TABLE IF NOT EXISTS search_log ("
        "search_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "user_id BIGINT UNSIGNED DEFAULT NULL,"
        "keyword VARCHAR(128) DEFAULT NULL,"
        "major VARCHAR(64) DEFAULT NULL,"
        "competition_level VARCHAR(64) DEFAULT NULL,"
        "signup_status VARCHAR(32) DEFAULT NULL,"
        "trigger_source VARCHAR(64) DEFAULT NULL,"
        "result_count INT NOT NULL DEFAULT 0,"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (search_id),"
        "KEY idx_search_log_user_time (user_id, create_time),"
        "CONSTRAINT fk_search_log_user FOREIGN KEY (user_id) REFERENCES user_info (user_id) ON DELETE SET NULL ON UPDATE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    ) && database_client_->execute(
        "CREATE TABLE IF NOT EXISTS navigation_command ("
        "command_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "user_id BIGINT UNSIGNED DEFAULT NULL,"
        "from_page VARCHAR(64) DEFAULT NULL,"
        "to_page VARCHAR(64) DEFAULT NULL,"
        "action_name VARCHAR(64) DEFAULT NULL,"
        "competition_id BIGINT UNSIGNED DEFAULT NULL,"
        "competition_name VARCHAR(128) DEFAULT NULL,"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "PRIMARY KEY (command_id),"
        "KEY idx_navigation_command_user_time (user_id, create_time),"
        "CONSTRAINT fk_navigation_command_user FOREIGN KEY (user_id) REFERENCES user_info (user_id) ON DELETE SET NULL ON UPDATE CASCADE,"
        "CONSTRAINT fk_navigation_command_competition FOREIGN KEY (competition_id) REFERENCES competition_info (competition_id) ON DELETE SET NULL ON UPDATE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    );

    if (!schema_ready) {
        return false;
    }

    const auto ensure_column = [&](const std::string& table_name, const std::string& column_name, const std::string& column_sql) {
        if (mysql_column_exists(database_client_, table_name, column_name)) {
            return true;
        }
        return database_client_->execute("ALTER TABLE `" + table_name + "` ADD COLUMN " + column_sql);
    };

    return ensure_column("search_log", "major", "`major` VARCHAR(64) DEFAULT NULL")
        && ensure_column("search_log", "competition_level", "`competition_level` VARCHAR(64) DEFAULT NULL")
        && ensure_column("search_log", "signup_status", "`signup_status` VARCHAR(32) DEFAULT NULL")
        && ensure_column("search_log", "trigger_source", "`trigger_source` VARCHAR(64) DEFAULT NULL")
        && ensure_column("search_log", "create_time", "`create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP")
        && ensure_column("navigation_command", "from_page", "`from_page` VARCHAR(64) DEFAULT NULL")
        && ensure_column("navigation_command", "to_page", "`to_page` VARCHAR(64) DEFAULT NULL")
        && ensure_column("navigation_command", "competition_id", "`competition_id` BIGINT UNSIGNED DEFAULT NULL")
        && ensure_column("navigation_command", "competition_name", "`competition_name` VARCHAR(128) DEFAULT NULL")
        && ensure_column("navigation_command", "create_time", "`create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP");
}

bool ActivityService::load_from_mysql() {
    search_records_.clear();
    navigation_records_.clear();
    next_search_id_ = 1;
    next_navigation_id_ = 1;

    const auto search_result = database_client_->query(
        "SELECT search_id, IFNULL(keyword, '') AS keyword, IFNULL(major, '') AS major, "
        "IFNULL(competition_level, '') AS competition_level, IFNULL(signup_status, '') AS signup_status, "
        "result_count, IFNULL(trigger_source, '') AS trigger_source, "
        "DATE_FORMAT(create_time, '%Y-%m-%d %H:%i:%s') AS create_time "
        "FROM search_log ORDER BY create_time ASC, search_id ASC"
    );
    const auto navigation_result = database_client_->query(
        "SELECT command_id, IFNULL(from_page, '') AS from_page, IFNULL(to_page, '') AS to_page, "
        "IFNULL(action_name, '') AS action_name, IFNULL(competition_id, 0) AS competition_id, "
        "IFNULL(competition_name, '') AS competition_name, "
        "DATE_FORMAT(create_time, '%Y-%m-%d %H:%i:%s') AS create_time "
        "FROM navigation_command ORDER BY create_time ASC, command_id ASC"
    );

    if (!search_result.has_value() || !navigation_result.has_value()) {
        return false;
    }

    for (const auto& row : search_result->rows) {
        SearchRecordItem item;
        item.search_id = parse_u64_or_zero(row.at("search_id"));
        item.keyword = row.at("keyword");
        item.major = row.at("major");
        item.competition_level = row.at("competition_level");
        item.signup_status = row.at("signup_status");
        item.result_count = persistence::parse_size(row.at("result_count")).value_or(0);
        item.trigger_source = row.at("trigger_source");
        item.create_time = row.at("create_time");
        search_records_.push_back(item);
        next_search_id_ = std::max(next_search_id_, item.search_id + 1);
    }

    for (const auto& row : navigation_result->rows) {
        NavigationRecordItem item;
        item.navigation_id = parse_u64_or_zero(row.at("command_id"));
        item.from_page = row.at("from_page");
        item.to_page = row.at("to_page");
        item.action_name = row.at("action_name");
        item.competition_id = parse_u64_or_zero(row.at("competition_id"));
        item.competition_name = row.at("competition_name");
        item.create_time = row.at("create_time");
        navigation_records_.push_back(item);
        next_navigation_id_ = std::max(next_navigation_id_, item.navigation_id + 1);
    }

    return true;
}

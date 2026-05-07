#include "competition_repository.h"
#include "persistence_utils.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

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

bool contains_ignore_case(const std::string& text, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return to_lower(text).find(to_lower(needle)) != std::string::npos;
}

bool equals_ignore_case(const std::string& left, const std::string& right) {
    if (right.empty()) {
        return true;
    }
    return to_lower(left) == to_lower(right);
}

std::uint64_t parse_u64_or_zero(const std::string& value) {
    const auto parsed = persistence::parse_u64(value);
    return parsed.has_value() ? parsed.value() : 0;
}

Category build_uncategorized_category(std::size_t competition_count) {
    Category category;
    category.category_id = 0;
    category.category_name = "Uncategorized";
    category.category_desc = "Fallback category for competitions whose original category was deleted.";
    category.sort_no = 0;
    category.competition_count = competition_count;
    category.update_time = "system";
    return category;
}

}  // namespace

CompetitionRepository::CompetitionRepository(MySqlClient* database_client)
    : database_client_(database_client),
      using_mysql_(database_client_ != nullptr && database_client_->available()),
      categories_file_path_(persistence::data_file_path("categories.tsv")),
      competitions_file_path_(persistence::data_file_path("competitions.tsv")) {
    seed_defaults();

    if (using_mysql_) {
        if (ensure_mysql_schema() && seed_mysql_defaults() && load_from_mysql()) {
            return;
        }

        using_mysql_ = false;
    }

    if (!load_persisted_state()) {
        persist();
    }
}

void CompetitionRepository::seed_defaults() {
    categories_ = {
        {1, "Programming", "Algorithm, programming and software development competitions", 10, 0, "2026-05-01 10:00:00"},
        {2, "Mathematical Modeling", "Modeling, statistics and optimization competitions", 20, 0, "2026-05-01 10:00:00"},
        {3, "Artificial Intelligence", "AI, machine learning and computer vision competitions", 30, 0, "2026-05-01 10:00:00"},
        {4, "Innovation and Entrepreneurship", "Innovation, startup and project practice competitions", 40, 0, "2026-05-01 10:00:00"},
    };

    competitions_ = {
        {
            1,
            1,
            "Programming",
            "ICPC International Collegiate Programming Contest",
            "ICPC Foundation",
            "International",
            "Open",
            "Computer Science, Software Engineering, Artificial Intelligence",
            "Sophomore and above",
            "2026-05-01 08:00:00",
            "2026-10-15 23:59:59",
            "2026-11-10 09:00:00",
            "2026-11-12 18:00:00",
            "https://icpc.global/",
            "ICPC is a well-known global programming contest that emphasizes algorithms, data structures and teamwork.",
            "Usually three students form one team. Strong algorithm and collaboration skills are recommended.",
            true,
            "2026-05-01 10:30:00",
            "2026-05-01 10:30:00",
        },
        {
            2,
            1,
            "Programming",
            "Lanqiao Cup Software and Information Technology Competition",
            "Talent Exchange Center of MIIT",
            "National",
            "Open",
            "Software Engineering, Computer Science, Artificial Intelligence, Electronic Information",
            "Freshman and above",
            "2026-05-10 09:00:00",
            "2026-11-01 23:59:59",
            "2026-12-05 09:00:00",
            "2026-12-05 17:30:00",
            "https://dasai.lanqiao.cn/",
            "Lanqiao Cup focuses on programming, embedded systems, MCU and other software engineering tracks.",
            "It is usually an individual competition and is friendly for students who want to improve through practice and past papers.",
            true,
            "2026-05-01 10:35:00",
            "2026-05-01 10:35:00",
        },
        {
            3,
            2,
            "Mathematical Modeling",
            "National College Student Mathematical Contest in Modeling",
            "China Society for Industrial and Applied Mathematics",
            "National A",
            "Open",
            "Mathematics, Statistics, Computer Science, Engineering",
            "Full-time university students",
            "2026-05-01 09:00:00",
            "2026-09-10 23:59:59",
            "2026-09-20 08:00:00",
            "2026-09-23 20:00:00",
            "https://www.mcm.edu.cn/",
            "This is a highly influential modeling contest in China and emphasizes modeling, coding and paper writing.",
            "Teams usually have three members. It is suitable for students with modeling, data analysis, programming and writing skills.",
            true,
            "2026-05-01 10:40:00",
            "2026-05-01 10:40:00",
        },
        {
            4,
            3,
            "Artificial Intelligence",
            "Global AI Innovation Challenge",
            "International AI Education Alliance",
            "International",
            "Warmup",
            "Artificial Intelligence, Data Science, Software Engineering, Digital Media Technology",
            "Freshman and above",
            "2026-07-01 00:00:00",
            "2026-08-20 23:59:59",
            "2026-09-05 09:00:00",
            "2026-09-07 18:00:00",
            "https://example.org/ai-challenge",
            "This competition emphasizes AI innovation, prototype design and interdisciplinary problem solving.",
            "Cross-major teams are allowed. Teams usually submit a prototype, written proposal and demo video.",
            true,
            "2026-05-01 10:45:00",
            "2026-05-01 10:45:00",
        },
        {
            5,
            4,
            "Innovation and Entrepreneurship",
            "Maker China University Innovation Competition",
            "Local industry and university alliance",
            "National",
            "Open",
            "All majors",
            "Undergraduate and postgraduate",
            "2026-05-15 09:00:00",
            "2026-10-30 23:59:59",
            "2026-11-20 09:00:00",
            "2026-11-25 18:00:00",
            "https://example.org/maker-china",
            "An innovation and entrepreneurship competition that highlights project value, teamwork and business presentation.",
            "Both individuals and teams can apply. It is suitable for students with prototypes, pitch decks or startup ideas.",
            true,
            "2026-05-01 10:50:00",
            "2026-05-01 10:50:00",
        },
    };

    next_category_id_ = 5;
    next_competition_id_ = 6;
}

bool CompetitionRepository::ensure_mysql_schema() {
    return database_client_->execute(
        "CREATE TABLE IF NOT EXISTS competition_category ("
        "category_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "category_name VARCHAR(64) NOT NULL,"
        "category_desc VARCHAR(255) DEFAULT NULL,"
        "sort_no INT NOT NULL DEFAULT 0,"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (category_id),"
        "UNIQUE KEY uk_competition_category_name (category_name),"
        "KEY idx_competition_category_sort_no (sort_no)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    ) && database_client_->execute(
        "CREATE TABLE IF NOT EXISTS competition_info ("
        "competition_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "competition_name VARCHAR(128) NOT NULL,"
        "category_id BIGINT UNSIGNED DEFAULT NULL,"
        "organizer VARCHAR(128) DEFAULT NULL,"
        "competition_level VARCHAR(32) DEFAULT NULL,"
        "signup_status VARCHAR(20) NOT NULL DEFAULT 'Open',"
        "suitable_major VARCHAR(128) DEFAULT NULL,"
        "suitable_grade VARCHAR(64) DEFAULT NULL,"
        "signup_start DATETIME DEFAULT NULL,"
        "signup_end DATETIME DEFAULT NULL,"
        "competition_start DATETIME DEFAULT NULL,"
        "competition_end DATETIME DEFAULT NULL,"
        "official_url VARCHAR(255) DEFAULT NULL,"
        "competition_desc TEXT DEFAULT NULL,"
        "participation_rules TEXT DEFAULT NULL,"
        "display_status TINYINT NOT NULL DEFAULT 1,"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (competition_id),"
        "KEY idx_competition_info_category (category_id),"
        "KEY idx_competition_info_level_status (competition_level, signup_status, display_status),"
        "CONSTRAINT fk_competition_info_category FOREIGN KEY (category_id) REFERENCES competition_category (category_id) "
        "ON DELETE SET NULL ON UPDATE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    );
}

bool CompetitionRepository::seed_mysql_defaults() {
    const auto result = database_client_->query("SELECT COUNT(*) AS total FROM competition_category");
    if (!result.has_value() || result->rows.empty()) {
        return false;
    }

    const std::uint64_t total = parse_u64_or_zero(result->rows.front().at("total"));
    if (total > 0) {
        return true;
    }

    for (const Category& category : categories_) {
        if (!database_client_->execute(
            "INSERT INTO competition_category (category_id, category_name, category_desc, sort_no, create_time, update_time) VALUES ("
            + std::to_string(category.category_id) + ","
            + database_client_->quote(category.category_name) + ","
            + database_client_->nullable_quote(category.category_desc) + ","
            + std::to_string(category.sort_no) + ","
            + database_client_->quote("2026-05-01 10:00:00") + ","
            + database_client_->quote(category.update_time) + ")"
        )) {
            return false;
        }
    }

    for (const Competition& competition : competitions_) {
        const std::string category_id_sql = competition.category_id == 0
            ? "NULL"
            : std::to_string(competition.category_id);

        if (!database_client_->execute(
            "INSERT INTO competition_info (competition_id, competition_name, category_id, organizer, competition_level, signup_status, "
            "suitable_major, suitable_grade, signup_start, signup_end, competition_start, competition_end, official_url, "
            "competition_desc, participation_rules, display_status, create_time, update_time) VALUES ("
            + std::to_string(competition.competition_id) + ","
            + database_client_->quote(competition.competition_name) + ","
            + category_id_sql + ","
            + database_client_->nullable_quote(competition.organizer) + ","
            + database_client_->nullable_quote(competition.competition_level) + ","
            + database_client_->quote(competition.signup_status) + ","
            + database_client_->nullable_quote(competition.suitable_major) + ","
            + database_client_->nullable_quote(competition.suitable_grade) + ","
            + database_client_->nullable_quote(competition.signup_start) + ","
            + database_client_->nullable_quote(competition.signup_end) + ","
            + database_client_->nullable_quote(competition.competition_start) + ","
            + database_client_->nullable_quote(competition.competition_end) + ","
            + database_client_->nullable_quote(competition.official_url) + ","
            + database_client_->nullable_quote(competition.competition_desc) + ","
            + database_client_->nullable_quote(competition.participation_rules) + ","
            + std::string(competition.display_status ? "1" : "0") + ","
            + database_client_->quote(competition.create_time) + ","
            + database_client_->quote(competition.update_time) + ")"
        )) {
            return false;
        }
    }

    return true;
}

bool CompetitionRepository::load_from_mysql() {
    const auto category_result = database_client_->query(
        "SELECT category_id, category_name, IFNULL(category_desc, '') AS category_desc, sort_no, "
        "DATE_FORMAT(update_time, '%Y-%m-%d %H:%i:%s') AS update_time "
        "FROM competition_category ORDER BY sort_no ASC, category_id ASC"
    );
    const auto competition_result = database_client_->query(
        "SELECT ci.competition_id, IFNULL(ci.category_id, 0) AS category_id, "
        "IFNULL(cc.category_name, 'Uncategorized') AS category_name, ci.competition_name, "
        "IFNULL(ci.organizer, '') AS organizer, IFNULL(ci.competition_level, '') AS competition_level, "
        "ci.signup_status, IFNULL(ci.suitable_major, '') AS suitable_major, IFNULL(ci.suitable_grade, '') AS suitable_grade, "
        "IFNULL(DATE_FORMAT(ci.signup_start, '%Y-%m-%d %H:%i:%s'), '') AS signup_start, "
        "IFNULL(DATE_FORMAT(ci.signup_end, '%Y-%m-%d %H:%i:%s'), '') AS signup_end, "
        "IFNULL(DATE_FORMAT(ci.competition_start, '%Y-%m-%d %H:%i:%s'), '') AS competition_start, "
        "IFNULL(DATE_FORMAT(ci.competition_end, '%Y-%m-%d %H:%i:%s'), '') AS competition_end, "
        "IFNULL(ci.official_url, '') AS official_url, IFNULL(ci.competition_desc, '') AS competition_desc, "
        "IFNULL(ci.participation_rules, '') AS participation_rules, ci.display_status, "
        "DATE_FORMAT(ci.create_time, '%Y-%m-%d %H:%i:%s') AS create_time, "
        "DATE_FORMAT(ci.update_time, '%Y-%m-%d %H:%i:%s') AS update_time "
        "FROM competition_info ci "
        "LEFT JOIN competition_category cc ON cc.category_id = ci.category_id "
        "ORDER BY ci.update_time DESC, ci.competition_id DESC"
    );

    if (!category_result.has_value() || !competition_result.has_value()) {
        return false;
    }

    categories_.clear();
    competitions_.clear();
    next_category_id_ = 1;
    next_competition_id_ = 1;

    for (const auto& row : category_result->rows) {
        Category category;
        category.category_id = parse_u64_or_zero(row.at("category_id"));
        category.category_name = row.at("category_name");
        category.category_desc = row.at("category_desc");
        category.sort_no = persistence::parse_int(row.at("sort_no")).value_or(0);
        category.update_time = row.at("update_time");
        categories_.push_back(category);
        next_category_id_ = std::max(next_category_id_, category.category_id + 1);
    }

    for (const auto& row : competition_result->rows) {
        Competition competition;
        competition.competition_id = parse_u64_or_zero(row.at("competition_id"));
        competition.category_id = parse_u64_or_zero(row.at("category_id"));
        competition.category_name = row.at("category_name");
        competition.competition_name = row.at("competition_name");
        competition.organizer = row.at("organizer");
        competition.competition_level = row.at("competition_level");
        competition.signup_status = row.at("signup_status");
        competition.suitable_major = row.at("suitable_major");
        competition.suitable_grade = row.at("suitable_grade");
        competition.signup_start = row.at("signup_start");
        competition.signup_end = row.at("signup_end");
        competition.competition_start = row.at("competition_start");
        competition.competition_end = row.at("competition_end");
        competition.official_url = row.at("official_url");
        competition.competition_desc = row.at("competition_desc");
        competition.participation_rules = row.at("participation_rules");
        competition.display_status = row.at("display_status") == "1";
        competition.create_time = row.at("create_time");
        competition.update_time = row.at("update_time");
        competitions_.push_back(competition);
        next_competition_id_ = std::max(next_competition_id_, competition.competition_id + 1);
    }

    return true;
}

bool CompetitionRepository::load_persisted_state() {
    if (!persistence::file_exists(categories_file_path_) || !persistence::file_exists(competitions_file_path_)) {
        return false;
    }

    std::vector<std::string> category_lines;
    std::vector<std::string> competition_lines;
    if (!persistence::read_lines(categories_file_path_, category_lines) || !persistence::read_lines(competitions_file_path_, competition_lines)) {
        return false;
    }

    std::vector<Category> loaded_categories;
    std::vector<Competition> loaded_competitions;
    std::vector<std::string> fields;
    std::uint64_t max_category_id = 0;
    std::uint64_t max_competition_id = 0;

    for (const std::string& line : category_lines) {
        if (line.empty()) {
            continue;
        }

        if (!persistence::split_fields(line, fields) || fields.size() != 5) {
            return false;
        }

        const auto category_id = persistence::parse_u64(fields[0]);
        const auto sort_no = persistence::parse_int(fields[3]);
        if (!category_id.has_value() || !sort_no.has_value()) {
            return false;
        }

        Category category;
        category.category_id = category_id.value();
        category.category_name = fields[1];
        category.category_desc = fields[2];
        category.sort_no = sort_no.value();
        category.update_time = fields[4];
        loaded_categories.push_back(category);
        max_category_id = std::max(max_category_id, category.category_id);
    }

    categories_ = loaded_categories;

    for (const std::string& line : competition_lines) {
        if (line.empty()) {
            continue;
        }

        if (!persistence::split_fields(line, fields) || fields.size() != 19) {
            return false;
        }

        const auto competition_id = persistence::parse_u64(fields[0]);
        const auto category_id = persistence::parse_u64(fields[1]);
        if (!competition_id.has_value() || !category_id.has_value()) {
            return false;
        }

        Competition competition;
        competition.competition_id = competition_id.value();
        competition.category_id = category_id.value();
        competition.category_name = fields[2];
        competition.competition_name = fields[3];
        competition.organizer = fields[4];
        competition.competition_level = fields[5];
        competition.signup_status = fields[6];
        competition.suitable_major = fields[7];
        competition.suitable_grade = fields[8];
        competition.signup_start = fields[9];
        competition.signup_end = fields[10];
        competition.competition_start = fields[11];
        competition.competition_end = fields[12];
        competition.official_url = fields[13];
        competition.competition_desc = fields[14];
        competition.participation_rules = fields[15];
        competition.display_status = persistence::parse_bool_flag(fields[16], true);
        competition.create_time = fields[17];
        competition.update_time = fields[18];
        apply_category_to_competition(competition, competition.category_id);
        loaded_competitions.push_back(competition);
        max_competition_id = std::max(max_competition_id, competition.competition_id);
    }

    competitions_ = loaded_competitions;
    next_category_id_ = max_category_id + 1;
    next_competition_id_ = max_competition_id + 1;
    return true;
}

bool CompetitionRepository::persist() const {
    if (using_mysql_) {
        return true;
    }

    std::vector<std::string> category_lines;
    std::vector<std::string> competition_lines;

    category_lines.reserve(categories_.size());
    for (const Category& category : categories_) {
        category_lines.push_back(persistence::join_fields({
            std::to_string(category.category_id),
            category.category_name,
            category.category_desc,
            std::to_string(category.sort_no),
            category.update_time
        }));
    }

    competition_lines.reserve(competitions_.size());
    for (const Competition& competition : competitions_) {
        competition_lines.push_back(persistence::join_fields({
            std::to_string(competition.competition_id),
            std::to_string(competition.category_id),
            competition.category_name,
            competition.competition_name,
            competition.organizer,
            competition.competition_level,
            competition.signup_status,
            competition.suitable_major,
            competition.suitable_grade,
            competition.signup_start,
            competition.signup_end,
            competition.competition_start,
            competition.competition_end,
            competition.official_url,
            competition.competition_desc,
            competition.participation_rules,
            persistence::bool_flag(competition.display_status),
            competition.create_time,
            competition.update_time
        }));
    }

    return persistence::write_lines(categories_file_path_, category_lines)
        && persistence::write_lines(competitions_file_path_, competition_lines);
}

std::vector<Competition> CompetitionRepository::list(const CompetitionFilter& filter) const {
    std::vector<Competition> result;

    for (const Competition& competition : competitions_) {
        if (!filter.include_hidden && !competition.display_status) {
            continue;
        }

        const bool keyword_match =
            contains_ignore_case(competition.competition_name, filter.keyword) ||
            contains_ignore_case(competition.category_name, filter.keyword) ||
            contains_ignore_case(competition.organizer, filter.keyword) ||
            contains_ignore_case(competition.competition_desc, filter.keyword);
        if (!keyword_match) {
            continue;
        }

        if (!contains_ignore_case(competition.suitable_major, filter.major)) {
            continue;
        }

        if (!contains_ignore_case(competition.competition_level, filter.competition_level)) {
            continue;
        }

        if (!equals_ignore_case(competition.signup_status, filter.signup_status)) {
            continue;
        }

        if (!contains_ignore_case(competition.suitable_grade, filter.suitable_grade)) {
            continue;
        }

        if (!contains_ignore_case(competition.category_name, filter.category_name)) {
            continue;
        }

        if (filter.category_id.has_value() && competition.category_id != filter.category_id.value()) {
            continue;
        }

        result.push_back(competition);
    }

    std::sort(result.begin(), result.end(), [](const Competition& left, const Competition& right) {
        return left.update_time > right.update_time;
    });

    return result;
}

std::optional<Competition> CompetitionRepository::find_by_id(std::uint64_t competition_id, bool include_hidden) const {
    const auto it = std::find_if(competitions_.begin(), competitions_.end(), [competition_id, include_hidden](const Competition& competition) {
        return competition.competition_id == competition_id && (include_hidden || competition.display_status);
    });

    if (it == competitions_.end()) {
        return std::nullopt;
    }

    return *it;
}

std::vector<Category> CompetitionRepository::list_categories() const {
    std::vector<Category> result;
    result.reserve(categories_.size() + 1);

    result.push_back(build_uncategorized_category(active_competition_count_for_category(0)));

    for (Category category : categories_) {
        category.competition_count = active_competition_count_for_category(category.category_id);
        result.push_back(category);
    }

    std::sort(result.begin(), result.end(), [](const Category& left, const Category& right) {
        if (left.sort_no != right.sort_no) {
            return left.sort_no < right.sort_no;
        }
        return left.category_id < right.category_id;
    });

    return result;
}

std::optional<Category> CompetitionRepository::find_category_by_id(std::uint64_t category_id) const {
    return category_snapshot(category_id);
}

std::optional<Category> CompetitionRepository::create_category(const CategoryDraft& draft, std::string& error_message) {
    const std::string category_name = trim_copy(draft.category_name);
    if (category_name.empty()) {
        error_message = "Category name is required.";
        return std::nullopt;
    }

    if (category_name_exists(category_name)) {
        error_message = "Category name already exists.";
        return std::nullopt;
    }

    if (using_mysql_) {
        if (!database_client_->execute(
            "INSERT INTO competition_category (category_name, category_desc, sort_no) VALUES ("
            + database_client_->quote(category_name) + ","
            + database_client_->nullable_quote(trim_copy(draft.category_desc)) + ","
            + std::to_string(draft.sort_no) + ")"
        )) {
            error_message = database_client_->last_error();
            return std::nullopt;
        }

        const std::uint64_t category_id = database_client_->last_insert_id();
        load_from_mysql();
        return category_snapshot(category_id);
    }

    Category category;
    category.category_id = next_category_id_++;
    category.category_name = category_name;
    category.category_desc = trim_copy(draft.category_desc);
    category.sort_no = draft.sort_no;
    category.update_time = current_timestamp();

    categories_.push_back(category);
    persist();
    return category_snapshot(category.category_id);
}

std::optional<Category> CompetitionRepository::update_category(std::uint64_t category_id, const CategoryDraft& draft, std::string& error_message) {
    const auto category_index = category_index_by_id(category_id);
    if (!category_index.has_value()) {
        error_message = "Category not found.";
        return std::nullopt;
    }

    const std::string category_name = trim_copy(draft.category_name);
    if (category_name.empty()) {
        error_message = "Category name is required.";
        return std::nullopt;
    }

    if (category_name_exists(category_name, category_id)) {
        error_message = "Category name already exists.";
        return std::nullopt;
    }

    if (using_mysql_) {
        if (!database_client_->execute(
            "UPDATE competition_category SET "
            "category_name = " + database_client_->quote(category_name) + ", "
            "category_desc = " + database_client_->nullable_quote(trim_copy(draft.category_desc)) + ", "
            "sort_no = " + std::to_string(draft.sort_no) + " "
            "WHERE category_id = " + std::to_string(category_id)
        )) {
            error_message = database_client_->last_error();
            return std::nullopt;
        }

        load_from_mysql();
        return category_snapshot(category_id);
    }

    Category& category = categories_[category_index.value()];
    category.category_name = category_name;
    category.category_desc = trim_copy(draft.category_desc);
    category.sort_no = draft.sort_no;
    category.update_time = current_timestamp();

    for (Competition& competition : competitions_) {
        if (competition.category_id == category_id) {
            competition.category_name = category.category_name;
            competition.update_time = category.update_time;
        }
    }

    persist();
    return category_snapshot(category_id);
}

bool CompetitionRepository::delete_category(std::uint64_t category_id, std::string& error_message) {
    const auto category_index = category_index_by_id(category_id);
    if (!category_index.has_value()) {
        error_message = "Category not found.";
        return false;
    }

    if (using_mysql_) {
        if (!database_client_->execute(
            "DELETE FROM competition_category WHERE category_id = " + std::to_string(category_id)
        )) {
            error_message = database_client_->last_error();
            return false;
        }

        load_from_mysql();
        return true;
    }

    categories_.erase(categories_.begin() + static_cast<std::ptrdiff_t>(category_index.value()));

    const std::string deleted_at = current_timestamp();
    for (Competition& competition : competitions_) {
        if (competition.category_id == category_id) {
            competition.category_id = 0;
            competition.category_name = "Uncategorized";
            competition.update_time = deleted_at;
        }
    }

    persist();
    return true;
}

std::optional<Competition> CompetitionRepository::create_competition(const CompetitionDraft& draft, std::string& error_message) {
    if (trim_copy(draft.competition_name).empty()) {
        error_message = "Competition name is required.";
        return std::nullopt;
    }

    if (draft.category_id != 0 && !category_index_by_id(draft.category_id).has_value()) {
        error_message = "Category does not exist.";
        return std::nullopt;
    }

    if (using_mysql_) {
        const std::string category_id_sql = draft.category_id == 0 ? "NULL" : std::to_string(draft.category_id);
        if (!database_client_->execute(
            "INSERT INTO competition_info (category_id, competition_name, organizer, competition_level, signup_status, "
            "suitable_major, suitable_grade, signup_start, signup_end, competition_start, competition_end, official_url, "
            "competition_desc, participation_rules, display_status) VALUES ("
            + category_id_sql + ","
            + database_client_->quote(trim_copy(draft.competition_name)) + ","
            + database_client_->nullable_quote(trim_copy(draft.organizer)) + ","
            + database_client_->nullable_quote(trim_copy(draft.competition_level)) + ","
            + database_client_->quote(trim_copy(draft.signup_status.empty() ? "Open" : draft.signup_status)) + ","
            + database_client_->nullable_quote(trim_copy(draft.suitable_major)) + ","
            + database_client_->nullable_quote(trim_copy(draft.suitable_grade)) + ","
            + database_client_->nullable_quote(trim_copy(draft.signup_start)) + ","
            + database_client_->nullable_quote(trim_copy(draft.signup_end)) + ","
            + database_client_->nullable_quote(trim_copy(draft.competition_start)) + ","
            + database_client_->nullable_quote(trim_copy(draft.competition_end)) + ","
            + database_client_->nullable_quote(trim_copy(draft.official_url)) + ","
            + database_client_->nullable_quote(trim_copy(draft.competition_desc)) + ","
            + database_client_->nullable_quote(trim_copy(draft.participation_rules)) + ","
            + std::string(draft.display_status ? "1" : "0") + ")"
        )) {
            error_message = database_client_->last_error();
            return std::nullopt;
        }

        const std::uint64_t competition_id = database_client_->last_insert_id();
        load_from_mysql();
        return find_by_id(competition_id, true);
    }

    const std::string timestamp = current_timestamp();
    Competition competition;
    competition.competition_id = next_competition_id_++;
    competition.category_id = draft.category_id;
    apply_category_to_competition(competition, draft.category_id);
    competition.competition_name = trim_copy(draft.competition_name);
    competition.organizer = trim_copy(draft.organizer);
    competition.competition_level = trim_copy(draft.competition_level);
    competition.signup_status = trim_copy(draft.signup_status.empty() ? "Open" : draft.signup_status);
    competition.suitable_major = trim_copy(draft.suitable_major);
    competition.suitable_grade = trim_copy(draft.suitable_grade);
    competition.signup_start = trim_copy(draft.signup_start);
    competition.signup_end = trim_copy(draft.signup_end);
    competition.competition_start = trim_copy(draft.competition_start);
    competition.competition_end = trim_copy(draft.competition_end);
    competition.official_url = trim_copy(draft.official_url);
    competition.competition_desc = trim_copy(draft.competition_desc);
    competition.participation_rules = trim_copy(draft.participation_rules);
    competition.display_status = draft.display_status;
    competition.create_time = timestamp;
    competition.update_time = timestamp;

    competitions_.push_back(competition);
    persist();
    return find_by_id(competition.competition_id, true);
}

std::optional<Competition> CompetitionRepository::update_competition(
    std::uint64_t competition_id,
    const CompetitionDraft& draft,
    std::string& error_message
) {
    const auto competition_index = competition_index_by_id(competition_id);
    if (!competition_index.has_value()) {
        error_message = "Competition not found.";
        return std::nullopt;
    }

    if (trim_copy(draft.competition_name).empty()) {
        error_message = "Competition name is required.";
        return std::nullopt;
    }

    if (draft.category_id != 0 && !category_index_by_id(draft.category_id).has_value()) {
        error_message = "Category does not exist.";
        return std::nullopt;
    }

    if (using_mysql_) {
        const std::string category_id_sql = draft.category_id == 0 ? "NULL" : std::to_string(draft.category_id);
        if (!database_client_->execute(
            "UPDATE competition_info SET "
            "category_id = " + category_id_sql + ", "
            "competition_name = " + database_client_->quote(trim_copy(draft.competition_name)) + ", "
            "organizer = " + database_client_->nullable_quote(trim_copy(draft.organizer)) + ", "
            "competition_level = " + database_client_->nullable_quote(trim_copy(draft.competition_level)) + ", "
            "signup_status = " + database_client_->quote(trim_copy(draft.signup_status.empty() ? "Open" : draft.signup_status)) + ", "
            "suitable_major = " + database_client_->nullable_quote(trim_copy(draft.suitable_major)) + ", "
            "suitable_grade = " + database_client_->nullable_quote(trim_copy(draft.suitable_grade)) + ", "
            "signup_start = " + database_client_->nullable_quote(trim_copy(draft.signup_start)) + ", "
            "signup_end = " + database_client_->nullable_quote(trim_copy(draft.signup_end)) + ", "
            "competition_start = " + database_client_->nullable_quote(trim_copy(draft.competition_start)) + ", "
            "competition_end = " + database_client_->nullable_quote(trim_copy(draft.competition_end)) + ", "
            "official_url = " + database_client_->nullable_quote(trim_copy(draft.official_url)) + ", "
            "competition_desc = " + database_client_->nullable_quote(trim_copy(draft.competition_desc)) + ", "
            "participation_rules = " + database_client_->nullable_quote(trim_copy(draft.participation_rules)) + ", "
            "display_status = " + std::string(draft.display_status ? "1" : "0") + " "
            "WHERE competition_id = " + std::to_string(competition_id)
        )) {
            error_message = database_client_->last_error();
            return std::nullopt;
        }

        load_from_mysql();
        return find_by_id(competition_id, true);
    }

    Competition& competition = competitions_[competition_index.value()];
    competition.category_id = draft.category_id;
    apply_category_to_competition(competition, draft.category_id);
    competition.competition_name = trim_copy(draft.competition_name);
    competition.organizer = trim_copy(draft.organizer);
    competition.competition_level = trim_copy(draft.competition_level);
    competition.signup_status = trim_copy(draft.signup_status.empty() ? "Open" : draft.signup_status);
    competition.suitable_major = trim_copy(draft.suitable_major);
    competition.suitable_grade = trim_copy(draft.suitable_grade);
    competition.signup_start = trim_copy(draft.signup_start);
    competition.signup_end = trim_copy(draft.signup_end);
    competition.competition_start = trim_copy(draft.competition_start);
    competition.competition_end = trim_copy(draft.competition_end);
    competition.official_url = trim_copy(draft.official_url);
    competition.competition_desc = trim_copy(draft.competition_desc);
    competition.participation_rules = trim_copy(draft.participation_rules);
    competition.display_status = draft.display_status;
    competition.update_time = current_timestamp();

    persist();
    return find_by_id(competition_id, true);
}

bool CompetitionRepository::delete_competition(std::uint64_t competition_id, std::string& error_message) {
    const auto competition_index = competition_index_by_id(competition_id);
    if (!competition_index.has_value()) {
        error_message = "Competition not found.";
        return false;
    }

    if (using_mysql_) {
        if (!database_client_->execute(
            "DELETE FROM competition_info WHERE competition_id = " + std::to_string(competition_id)
        )) {
            error_message = database_client_->last_error();
            return false;
        }

        load_from_mysql();
        return true;
    }

    competitions_.erase(competitions_.begin() + static_cast<std::ptrdiff_t>(competition_index.value()));
    persist();
    return true;
}

std::string CompetitionRepository::current_timestamp() {
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

std::optional<std::size_t> CompetitionRepository::category_index_by_id(std::uint64_t category_id) const {
    for (std::size_t index = 0; index < categories_.size(); ++index) {
        if (categories_[index].category_id == category_id) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> CompetitionRepository::competition_index_by_id(std::uint64_t competition_id) const {
    for (std::size_t index = 0; index < competitions_.size(); ++index) {
        if (competitions_[index].competition_id == competition_id) {
            return index;
        }
    }
    return std::nullopt;
}

bool CompetitionRepository::category_name_exists(const std::string& category_name, std::optional<std::uint64_t> exclude_id) const {
    const std::string normalized_name = to_lower(trim_copy(category_name));
    return std::any_of(categories_.begin(), categories_.end(), [&](const Category& category) {
        if (exclude_id.has_value() && category.category_id == exclude_id.value()) {
            return false;
        }
        return to_lower(trim_copy(category.category_name)) == normalized_name;
    });
}

std::optional<Category> CompetitionRepository::category_snapshot(std::uint64_t category_id) const {
    if (category_id == 0) {
        return build_uncategorized_category(active_competition_count_for_category(0));
    }

    const auto category_index = category_index_by_id(category_id);
    if (!category_index.has_value()) {
        return std::nullopt;
    }

    Category category = categories_[category_index.value()];
    category.competition_count = active_competition_count_for_category(category.category_id);
    return category;
}

std::size_t CompetitionRepository::active_competition_count_for_category(std::uint64_t category_id) const {
    return static_cast<std::size_t>(std::count_if(competitions_.begin(), competitions_.end(), [category_id](const Competition& competition) {
        return competition.category_id == category_id && competition.display_status;
    }));
}

void CompetitionRepository::apply_category_to_competition(Competition& competition, std::uint64_t category_id) const {
    const auto category = category_snapshot(category_id);
    if (!category.has_value()) {
        competition.category_id = 0;
        competition.category_name = "Uncategorized";
        return;
    }

    competition.category_id = category.value().category_id;
    competition.category_name = category.value().category_name;
}

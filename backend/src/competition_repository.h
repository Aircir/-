#pragma once

#include "mysql_client.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Category {
    std::uint64_t category_id = 0;
    std::string category_name;
    std::string category_desc;
    int sort_no = 0;
    std::size_t competition_count = 0;
    std::string update_time;
};

struct Competition {
    std::uint64_t competition_id = 0;
    std::uint64_t category_id = 0;
    std::string category_name;
    std::string competition_name;
    std::string organizer;
    std::string competition_level;
    std::string signup_status;
    std::string suitable_major;
    std::string suitable_grade;
    std::string signup_start;
    std::string signup_end;
    std::string competition_start;
    std::string competition_end;
    std::string official_url;
    std::string competition_desc;
    std::string participation_rules;
    bool display_status = true;
    std::string create_time;
    std::string update_time;
};

struct CategoryDraft {
    std::string category_name;
    std::string category_desc;
    int sort_no = 0;
};

struct CompetitionDraft {
    std::uint64_t category_id = 0;
    std::string competition_name;
    std::string organizer;
    std::string competition_level;
    std::string signup_status;
    std::string suitable_major;
    std::string suitable_grade;
    std::string signup_start;
    std::string signup_end;
    std::string competition_start;
    std::string competition_end;
    std::string official_url;
    std::string competition_desc;
    std::string participation_rules;
    bool display_status = true;
};

struct CompetitionFilter {
    std::string keyword;
    std::string major;
    std::string competition_level;
    std::string signup_status;
    std::string suitable_grade;
    std::string category_name;
    std::optional<std::uint64_t> category_id;
    bool include_hidden = false;
};

class CompetitionRepository {
public:
    explicit CompetitionRepository(MySqlClient* database_client = nullptr);

    std::vector<Competition> list(const CompetitionFilter& filter) const;
    std::optional<Competition> find_by_id(std::uint64_t competition_id, bool include_hidden = false) const;

    std::vector<Category> list_categories() const;
    std::optional<Category> find_category_by_id(std::uint64_t category_id) const;

    std::optional<Category> create_category(const CategoryDraft& draft, std::string& error_message);
    std::optional<Category> update_category(std::uint64_t category_id, const CategoryDraft& draft, std::string& error_message);
    bool delete_category(std::uint64_t category_id, std::string& error_message);

    std::optional<Competition> create_competition(const CompetitionDraft& draft, std::string& error_message);
    std::optional<Competition> update_competition(std::uint64_t competition_id, const CompetitionDraft& draft, std::string& error_message);
    bool delete_competition(std::uint64_t competition_id, std::string& error_message);

private:
    MySqlClient* database_client_ = nullptr;
    bool using_mysql_ = false;
    std::vector<Category> categories_;
    std::vector<Competition> competitions_;
    std::uint64_t next_category_id_ = 1;
    std::uint64_t next_competition_id_ = 1;
    std::string categories_file_path_;
    std::string competitions_file_path_;

    static std::string current_timestamp();

    void seed_defaults();
    bool ensure_mysql_schema();
    bool seed_mysql_defaults();
    bool load_from_mysql();
    bool load_persisted_state();
    bool persist() const;
    std::optional<std::size_t> category_index_by_id(std::uint64_t category_id) const;
    std::optional<std::size_t> competition_index_by_id(std::uint64_t competition_id) const;
    bool category_name_exists(const std::string& category_name, std::optional<std::uint64_t> exclude_id = std::nullopt) const;
    std::optional<Category> category_snapshot(std::uint64_t category_id) const;
    std::size_t active_competition_count_for_category(std::uint64_t category_id) const;
    void apply_category_to_competition(Competition& competition, std::uint64_t category_id) const;
};

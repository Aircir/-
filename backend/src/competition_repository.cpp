#include "competition_repository.h"

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

CompetitionRepository::CompetitionRepository()
    : categories_({
          {1, "Programming", "Algorithm, programming and software development competitions", 10, 0, "2026-05-01 10:00:00"},
          {2, "Mathematical Modeling", "Modeling, statistics and optimization competitions", 20, 0, "2026-05-01 10:00:00"},
          {3, "Artificial Intelligence", "AI, machine learning and computer vision competitions", 30, 0, "2026-05-01 10:00:00"},
          {4, "Innovation and Entrepreneurship", "Innovation, startup and project practice competitions", 40, 0, "2026-05-01 10:00:00"},
      }),
      competitions_({
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
      }),
      next_category_id_(5),
      next_competition_id_(6) {}

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

    Category category;
    category.category_id = next_category_id_++;
    category.category_name = category_name;
    category.category_desc = trim_copy(draft.category_desc);
    category.sort_no = draft.sort_no;
    category.update_time = current_timestamp();

    categories_.push_back(category);
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

    return category_snapshot(category_id);
}

bool CompetitionRepository::delete_category(std::uint64_t category_id, std::string& error_message) {
    const auto category_index = category_index_by_id(category_id);
    if (!category_index.has_value()) {
        error_message = "Category not found.";
        return false;
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

    return find_by_id(competition_id, true);
}

bool CompetitionRepository::delete_competition(std::uint64_t competition_id, std::string& error_message) {
    const auto competition_index = competition_index_by_id(competition_id);
    if (!competition_index.has_value()) {
        error_message = "Competition not found.";
        return false;
    }

    competitions_.erase(competitions_.begin() + static_cast<std::ptrdiff_t>(competition_index.value()));
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

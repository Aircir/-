#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace persistence {

inline std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

inline std::string data_root_directory() {
    const char* raw = std::getenv("COMPETITION_DATA_DIR");
    const std::string configured = raw == nullptr ? "" : trim_copy(raw);
    return configured.empty() ? "data/runtime" : configured;
}

inline std::string data_file_path(const std::string& filename) {
    return data_root_directory() + "/" + filename;
}

inline bool file_exists(const std::string& path) {
    std::error_code error;
    return std::filesystem::exists(std::filesystem::path(path), error);
}

inline bool ensure_parent_directory(const std::string& path) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        return std::filesystem::exists(parent, error);
    }
    return true;
}

inline std::string escape_field(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (unsigned char ch : value) {
        switch (ch) {
            case '\\':
                result += "\\\\";
                break;
            case '\t':
                result += "\\t";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            default:
                result.push_back(static_cast<char>(ch));
                break;
        }
    }

    return result;
}

inline bool split_fields(const std::string& line, std::vector<std::string>& fields) {
    fields.clear();
    std::string current;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];

        if (ch == '\\') {
            if (index + 1 >= line.size()) {
                return false;
            }

            const char escaped = line[++index];
            switch (escaped) {
                case '\\':
                    current.push_back('\\');
                    break;
                case 't':
                    current.push_back('\t');
                    break;
                case 'n':
                    current.push_back('\n');
                    break;
                case 'r':
                    current.push_back('\r');
                    break;
                default:
                    return false;
            }
            continue;
        }

        if (ch == '\t') {
            fields.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    fields.push_back(current);
    return true;
}

inline std::string join_fields(const std::vector<std::string>& fields) {
    std::string line;

    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0) {
            line.push_back('\t');
        }
        line += escape_field(fields[index]);
    }

    return line;
}

inline bool read_lines(const std::string& path, std::vector<std::string>& lines) {
    lines.clear();

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    return input.good() || input.eof();
}

inline bool write_lines(const std::string& path, const std::vector<std::string>& lines) {
    if (!ensure_parent_directory(path)) {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    for (const std::string& line : lines) {
        output << line << '\n';
    }

    return output.good();
}

inline std::optional<std::uint64_t> parse_u64(const std::string& value) {
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

inline std::optional<std::size_t> parse_size(const std::string& value) {
    const auto parsed = parse_u64(value);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(parsed.value());
}

inline std::optional<int> parse_int(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return std::nullopt;
    }
}

inline bool parse_bool_flag(const std::string& value, bool default_value = false) {
    if (value.empty()) {
        return default_value;
    }

    std::string normalized = trim_copy(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

inline std::string bool_flag(bool value) {
    return value ? "1" : "0";
}

}  // namespace persistence

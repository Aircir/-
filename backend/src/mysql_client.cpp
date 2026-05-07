#include "mysql_client.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

#include <mysql.h>

namespace {

std::string fallback_text(const std::string& value, const std::string& fallback) {
    return value.empty() ? fallback : value;
}

}  // namespace

MySqlClient::MySqlClient()
    : host_(fallback_text(trim_copy(read_env_value("MYSQL_HOST")), "127.0.0.1")),
      user_(fallback_text(trim_copy(read_env_value("MYSQL_USER")), "root")),
      password_(read_env_value("MYSQL_PASSWORD")),
      database_(fallback_text(trim_copy(read_env_value("MYSQL_DATABASE")), "college_competition_ai")),
      port_(read_env_port()) {
    available_ = connect();
}

MySqlClient::~MySqlClient() {
    close();
}

bool MySqlClient::available() const {
    return available_ && connection_ != nullptr;
}

const std::string& MySqlClient::last_error() const {
    return last_error_;
}

const std::string& MySqlClient::database_name() const {
    return database_;
}

bool MySqlClient::execute(const std::string& sql) {
    if (!available()) {
        set_error("MySQL connection is not available.");
        return false;
    }

    if (mysql_query(reinterpret_cast<MYSQL*>(connection_), sql.c_str()) != 0) {
        set_error(mysql_error(reinterpret_cast<MYSQL*>(connection_)));
        return false;
    }

    return true;
}

std::optional<MySqlQueryResult> MySqlClient::query(const std::string& sql) {
    if (!available()) {
        set_error("MySQL connection is not available.");
        return std::nullopt;
    }

    MYSQL* mysql = reinterpret_cast<MYSQL*>(connection_);
    if (mysql_query(mysql, sql.c_str()) != 0) {
        set_error(mysql_error(mysql));
        return std::nullopt;
    }

    MYSQL_RES* result = mysql_store_result(mysql);
    if (result == nullptr) {
        if (mysql_field_count(mysql) == 0) {
            return MySqlQueryResult{};
        }

        set_error(mysql_error(mysql));
        return std::nullopt;
    }

    MySqlQueryResult query_result;
    const unsigned int field_count = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        std::unordered_map<std::string, std::string> item;

        for (unsigned int index = 0; index < field_count; ++index) {
            const std::string key = fields[index].name == nullptr ? "" : fields[index].name;
            const char* raw = row[index];
            if (raw == nullptr) {
                item[key] = "";
            } else {
                item[key] = std::string(raw, lengths[index]);
            }
        }

        query_result.rows.push_back(std::move(item));
    }

    mysql_free_result(result);
    return query_result;
}

std::string MySqlClient::escape(const std::string& value) {
    if (!available()) {
        return value;
    }

    MYSQL* mysql = reinterpret_cast<MYSQL*>(connection_);
    std::string output(value.size() * 2 + 1, '\0');
    const unsigned long written = mysql_real_escape_string(
        mysql,
        output.data(),
        value.c_str(),
        static_cast<unsigned long>(value.size())
    );
    output.resize(written);
    return output;
}

std::string MySqlClient::quote(const std::string& value) {
    return "'" + escape(value) + "'";
}

std::string MySqlClient::nullable_quote(const std::string& value) {
    return trim_copy(value).empty() ? "NULL" : quote(value);
}

std::uint64_t MySqlClient::last_insert_id() const {
    if (!available()) {
        return 0;
    }

    return static_cast<std::uint64_t>(mysql_insert_id(reinterpret_cast<MYSQL*>(connection_)));
}

std::string MySqlClient::trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string MySqlClient::read_env_value(const char* name) {
    const char* raw = std::getenv(name);
    return raw == nullptr ? "" : raw;
}

unsigned int MySqlClient::read_env_port() {
    const std::string raw = trim_copy(read_env_value("MYSQL_PORT"));
    if (raw.empty()) {
        return 3306;
    }

    try {
        const int value = std::stoi(raw);
        return value > 0 ? static_cast<unsigned int>(value) : 3306;
    } catch (...) {
        return 3306;
    }
}

bool MySqlClient::connect() {
    close();

    MYSQL* mysql = mysql_init(nullptr);
    if (mysql == nullptr) {
        set_error("mysql_init failed.");
        return false;
    }

    connection_ = reinterpret_cast<st_mysql*>(mysql);

    if (mysql_real_connect(mysql, host_.c_str(), user_.c_str(), password_.c_str(), nullptr, port_, nullptr, 0) == nullptr) {
        set_error(mysql_error(mysql));
        close();
        return false;
    }

    if (mysql_query(mysql, "SET NAMES utf8mb4") != 0) {
        set_error(mysql_error(mysql));
        close();
        return false;
    }

    std::ostringstream create_database_sql;
    create_database_sql
        << "CREATE DATABASE IF NOT EXISTS `" << database_
        << "` DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci";
    if (mysql_query(mysql, create_database_sql.str().c_str()) != 0) {
        set_error(mysql_error(mysql));
        close();
        return false;
    }

    if (mysql_select_db(mysql, database_.c_str()) != 0) {
        set_error(mysql_error(mysql));
        close();
        return false;
    }

    last_error_.clear();
    return true;
}

void MySqlClient::close() {
    if (connection_ != nullptr) {
        mysql_close(reinterpret_cast<MYSQL*>(connection_));
        connection_ = nullptr;
    }
    available_ = false;
}

void MySqlClient::set_error(const std::string& message) {
    last_error_ = message;
}

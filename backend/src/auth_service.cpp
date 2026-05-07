#include "auth_service.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

std::string fallback_text(const std::string& value, const std::string& fallback) {
    return value.empty() ? fallback : value;
}

std::string role_from_user_type(const std::string& user_type) {
    return user_type == "2" ? "admin" : "student";
}

int user_type_from_role(const std::string& role) {
    return role == "admin" ? 2 : 1;
}

}  // namespace

AuthService::AuthService(MySqlClient* database_client)
    : database_client_(database_client),
      using_mysql_(database_client_ != nullptr && database_client_->available()) {
    const std::string admin_username = fallback_text(trim_copy(read_env_value("ADMIN_USERNAME")), "admin");
    const std::string admin_password = fallback_text(trim_copy(read_env_value("ADMIN_PASSWORD")), "admin123");
    const std::string student_username = fallback_text(trim_copy(read_env_value("STUDENT_USERNAME")), "student");
    const std::string student_password = fallback_text(trim_copy(read_env_value("STUDENT_PASSWORD")), "123456");

    accounts_.push_back({student_username, student_password, std::string(u8"学生演示账号"), "student"});
    accounts_.push_back({admin_username, admin_password, std::string(u8"管理员演示账号"), "admin"});

    if (using_mysql_) {
        if (!ensure_mysql_schema() || !seed_mysql_accounts()) {
            using_mysql_ = false;
        }
    }
}

std::optional<AuthLoginResult> AuthService::login(
    const std::string& username,
    const std::string& password,
    std::string& error_message
) {
    const std::string normalized_username = trim_copy(username);
    const std::string normalized_password = trim_copy(password);
    if (normalized_username.empty() || normalized_password.empty()) {
        error_message = std::string(u8"请输入用户名和密码。");
        return std::nullopt;
    }

    AuthUser user;
    bool matched_user = false;

    if (using_mysql_) {
        const auto result = database_client_->query(
            "SELECT user_id, username, password, IFNULL(real_name, '') AS real_name, user_type, status "
            "FROM user_info WHERE username = " + database_client_->quote(normalized_username) + " LIMIT 1"
        );

        if (result.has_value() && !result->rows.empty()) {
            const auto& row = result->rows.front();
            if (row.at("status") == "1" && row.at("password") == normalized_password) {
                matched_user = true;
                try {
                    user.user_id = static_cast<std::uint64_t>(std::stoull(row.at("user_id")));
                } catch (...) {
                    user.user_id = 0;
                }
                user.username = row.at("username");
                user.display_name = fallback_text(row.at("real_name"), row.at("username"));
                user.role = role_from_user_type(row.at("user_type"));
            }
        }
    } else {
        const auto matched = std::find_if(accounts_.begin(), accounts_.end(), [&](const DemoAccount& account) {
            return to_lower(account.username) == to_lower(normalized_username)
                && account.password == normalized_password;
        });

        if (matched != accounts_.end()) {
            matched_user = true;
            user.user_id = matched->role == "admin" ? 2 : 1;
            user.username = matched->username;
            user.display_name = matched->display_name;
            user.role = matched->role;
        }
    }

    if (!matched_user) {
        error_message = std::string(u8"用户名或密码错误。");
        return std::nullopt;
    }

    AuthLoginResult result;
    result.token = next_token_text();
    result.user = user;
    result.login_time = current_timestamp();

    active_sessions_[result.token] = {result.user, result.login_time};
    return result;
}

std::optional<AuthUser> AuthService::authenticate(const std::string& token) const {
    const std::string normalized_token = trim_copy(token);
    if (normalized_token.empty()) {
        return std::nullopt;
    }

    const auto session = active_sessions_.find(normalized_token);
    if (session == active_sessions_.end()) {
        return std::nullopt;
    }

    return session->second.user;
}

bool AuthService::logout(const std::string& token) {
    const std::string normalized_token = trim_copy(token);
    if (normalized_token.empty()) {
        return false;
    }

    return active_sessions_.erase(normalized_token) > 0;
}

std::string AuthService::current_timestamp() {
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

std::string AuthService::trim_copy(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string AuthService::to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string AuthService::read_env_value(const char* name) {
    const char* raw = std::getenv(name);
    return raw == nullptr ? "" : raw;
}

bool AuthService::ensure_mysql_schema() {
    return database_client_->execute(
        "CREATE TABLE IF NOT EXISTS user_info ("
        "user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "username VARCHAR(50) NOT NULL,"
        "password VARCHAR(255) NOT NULL,"
        "real_name VARCHAR(50) DEFAULT NULL,"
        "major VARCHAR(50) DEFAULT NULL,"
        "grade VARCHAR(20) DEFAULT NULL,"
        "user_type TINYINT NOT NULL DEFAULT 1,"
        "status TINYINT NOT NULL DEFAULT 1,"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (user_id),"
        "UNIQUE KEY uk_user_info_username (username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    );
}

bool AuthService::seed_mysql_accounts() {
    for (const DemoAccount& account : accounts_) {
        if (!database_client_->execute(
            "INSERT INTO user_info (username, password, real_name, user_type, status) VALUES ("
            + database_client_->quote(account.username) + ","
            + database_client_->quote(account.password) + ","
            + database_client_->quote(account.display_name) + ","
            + std::to_string(user_type_from_role(account.role)) + ",1)"
            " ON DUPLICATE KEY UPDATE "
            "password = VALUES(password), "
            "real_name = VALUES(real_name), "
            "user_type = VALUES(user_type), "
            "status = 1"
        )) {
            return false;
        }
    }

    return true;
}

std::string AuthService::next_token_text() {
    return "token-" + std::to_string(next_token_id_++) + "-" + std::to_string(std::time(nullptr));
}

#pragma once

#include "mysql_client.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct AuthUser {
    std::uint64_t user_id = 0;
    std::string username;
    std::string display_name;
    std::string role;
};

struct AuthLoginResult {
    std::string token;
    AuthUser user;
    std::string login_time;
};

class AuthService {
public:
    explicit AuthService(MySqlClient* database_client = nullptr);

    std::optional<AuthLoginResult> login(
        const std::string& username,
        const std::string& password,
        std::string& error_message
    );

    std::optional<AuthUser> authenticate(const std::string& token) const;
    bool logout(const std::string& token);

private:
    struct DemoAccount {
        std::string username;
        std::string password;
        std::string display_name;
        std::string role;
    };

    struct ActiveSession {
        AuthUser user;
        std::string login_time;
    };

    MySqlClient* database_client_ = nullptr;
    bool using_mysql_ = false;
    std::vector<DemoAccount> accounts_;
    std::unordered_map<std::string, ActiveSession> active_sessions_;
    std::uint64_t next_token_id_ = 1;

    static std::string current_timestamp();
    static std::string trim_copy(std::string value);
    static std::string to_lower(std::string value);
    static std::string read_env_value(const char* name);
    bool ensure_mysql_schema();
    bool seed_mysql_accounts();
    std::string next_token_text();
};

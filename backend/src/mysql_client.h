#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct st_mysql;

struct MySqlQueryResult {
    std::vector<std::unordered_map<std::string, std::string>> rows;
};

class MySqlClient {
public:
    MySqlClient();
    ~MySqlClient();

    bool available() const;
    const std::string& last_error() const;
    const std::string& database_name() const;

    bool execute(const std::string& sql);
    std::optional<MySqlQueryResult> query(const std::string& sql);
    std::string escape(const std::string& value);
    std::string quote(const std::string& value);
    std::string nullable_quote(const std::string& value);
    std::uint64_t last_insert_id() const;

private:
    st_mysql* connection_ = nullptr;
    bool available_ = false;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    unsigned int port_ = 3306;
    std::string last_error_;

    static std::string trim_copy(std::string value);
    static std::string read_env_value(const char* name);
    static unsigned int read_env_port();
    bool connect();
    void close();
    void set_error(const std::string& message);
};

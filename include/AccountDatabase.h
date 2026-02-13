#pragma once

#include <string>
#include <optional>
#include <mutex>
#include <sqlite3.h>

class AccountDatabase {
public:
    AccountDatabase(const std::string& dbPath = "freia_accounts.db");
    ~AccountDatabase();

    bool createAccount(const std::string& username, const std::string& keyBase64);

    std::optional<std::string> validateLogin(const std::string& username, const std::string& keyBase64);

    bool userExists(const std::string& username) const;

    bool deleteAccount(const std::string& username);

    size_t getAccountCount() const;

private:
    sqlite3* db = nullptr;
    mutable std::mutex dbMutex;

    bool execute(const std::string& sql);
    bool prepareAndStep(const std::string& sql, sqlite3_stmt** stmtOut = nullptr);
    std::optional<std::string> querySingleString(const std::string& sql);

    bool initializeSchema();
};
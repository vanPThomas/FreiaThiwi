#pragma once

#include <string>
#include <optional>
#include <mutex>
#include <sqlite3.h>

class AccountDatabase {
public:
    AccountDatabase(const std::string& dbPath = "freia_accounts.db");
    ~AccountDatabase();

    // Returns true if account was created, false if username already exists
    bool createAccount(const std::string& username, const std::string& keyBase64);

    // Returns the stored base64 key if login succeeds, std::nullopt otherwise
    std::optional<std::string> validateLogin(const std::string& username, const std::string& keyBase64);

    // Optional: check if user exists (for UI hints later)
    bool userExists(const std::string& username) const;

    // Optional: delete account (for admin later)
    bool deleteAccount(const std::string& username);

    // Optional: get number of accounts (stats/logging)
    size_t getAccountCount() const;

private:
    sqlite3* db = nullptr;
    mutable std::mutex dbMutex;  // protects sqlite3 calls (SQLite is thread-safe with mutex)

    bool execute(const std::string& sql);
    bool prepareAndStep(const std::string& sql, sqlite3_stmt** stmtOut = nullptr);
    std::optional<std::string> querySingleString(const std::string& sql);

    // Helper to initialize schema on first open
    bool initializeSchema();
};
#include "AccountDatabase.h"
#include <iostream>

AccountDatabase::AccountDatabase(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        db = nullptr;
        return;
    }

    // Enable WAL mode for better concurrency
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    if (!initializeSchema()) {
        std::cerr << "Failed to initialize account database schema\n";
        sqlite3_close(db);
        db = nullptr;
    }
}

AccountDatabase::~AccountDatabase() {
    if (db) sqlite3_close(db);
}

bool AccountDatabase::initializeSchema() {
    const char* createTable = 
        "CREATE TABLE IF NOT EXISTS accounts ("
        "  username TEXT PRIMARY KEY,"
        "  key_base64 TEXT NOT NULL"
        ");";

    return execute(createTable);
}

bool AccountDatabase::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(dbMutex);
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool AccountDatabase::createAccount(const std::string& username, const std::string& keyBase64) {
    std::lock_guard<std::mutex> lock(dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR IGNORE INTO accounts (username, key_base64) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, keyBase64.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::optional<std::string> AccountDatabase::validateLogin(const std::string& username, const std::string& keyBase64) {
    std::lock_guard<std::mutex> lock(dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT key_base64 FROM accounts WHERE username = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<std::string> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* storedKey = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result = std::string(storedKey ? storedKey : "");
    }

    sqlite3_finalize(stmt);

    if (result && *result == keyBase64) {
        return *result;
    }
    return std::nullopt;
}
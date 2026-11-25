#include "RoomDatabase.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <array>
#include <iostream>

namespace {
constexpr int kSaltLen = 16;
constexpr int kVerifierLen = 32;
constexpr int kIterations = 150000;

bool makeVerifier(const std::string& key, const unsigned char* salt,
                  std::array<unsigned char, kVerifierLen>& verifier) {
    return PKCS5_PBKDF2_HMAC(key.data(), static_cast<int>(key.size()),
                             salt, kSaltLen, kIterations, EVP_sha256(),
                             kVerifierLen, verifier.data()) == 1;
}

struct Statement {
    sqlite3_stmt* stmt = nullptr;
    Statement(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
            stmt = nullptr;
    }
    ~Statement() { if (stmt) sqlite3_finalize(stmt); }
    explicit operator bool() const { return stmt != nullptr; }
};

std::string columnString(sqlite3_stmt* stmt, int col) {
    const auto* p = sqlite3_column_text(stmt, col);
    return p ? reinterpret_cast<const char*>(p) : "";
}
}

RoomDatabase::RoomDatabase(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Cannot open room database: " << sqlite3_errmsg(db) << '\n';
        sqlite3_close(db);
        db = nullptr;
        return;
    }
    sqlite3_busy_timeout(db, 5000);
    if (!initialize()) {
        std::cerr << "Cannot initialize room database: " << sqlite3_errmsg(db) << '\n';
        sqlite3_close(db);
        db = nullptr;
    }
}

RoomDatabase::~RoomDatabase() { if (db) sqlite3_close(db); }

bool RoomDatabase::initialize() {
    const char* sql = R"SQL(
        PRAGMA journal_mode=WAL;
        PRAGMA foreign_keys=ON;
        CREATE TABLE IF NOT EXISTS rooms (
            name TEXT PRIMARY KEY,
            creator TEXT NOT NULL REFERENCES accounts(username),
            created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
            password_salt BLOB NOT NULL,
            password_verifier BLOB NOT NULL
        );
        CREATE TABLE IF NOT EXISTS room_members (
            room_name TEXT NOT NULL REFERENCES rooms(name) ON DELETE CASCADE,
            username TEXT NOT NULL REFERENCES accounts(username),
            first_joined_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
            PRIMARY KEY(room_name, username)
        );
        CREATE TABLE IF NOT EXISTS room_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            room_name TEXT NOT NULL REFERENCES rooms(name) ON DELETE CASCADE,
            sender TEXT NOT NULL REFERENCES accounts(username),
            ciphertext_base64 TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
        );
        CREATE INDEX IF NOT EXISTS idx_room_messages_room_id
            ON room_messages(room_name, id);
    )SQL";
    char* error = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite room schema: " << (error ? error : "unknown") << '\n';
        sqlite3_free(error);
    }
    return rc == SQLITE_OK;
}

bool RoomDatabase::createRoom(const std::string& name, const std::string& creator,
                              const std::string& authTokenBase64) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!db) return false;
    std::array<unsigned char, kSaltLen> salt{};
    std::array<unsigned char, kVerifierLen> verifier{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        !makeVerifier(authTokenBase64, salt.data(), verifier)) return false;

    Statement s(db, "INSERT INTO rooms(name, creator, password_salt, password_verifier) VALUES(?,?,?,?)");
    if (!s) return false;
    sqlite3_bind_text(s.stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, creator.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(s.stmt, 3, salt.data(), kSaltLen, SQLITE_TRANSIENT);
    sqlite3_bind_blob(s.stmt, 4, verifier.data(), kVerifierLen, SQLITE_TRANSIENT);
    return sqlite3_step(s.stmt) == SQLITE_DONE;
}

std::optional<StoredRoom> RoomDatabase::findRoom(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!db) return std::nullopt;
    Statement s(db, "SELECT name, creator, created_at FROM rooms WHERE name=?");
    if (!s) return std::nullopt;
    sqlite3_bind_text(s.stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s.stmt) != SQLITE_ROW) return std::nullopt;
    return StoredRoom{columnString(s.stmt, 0), columnString(s.stmt, 1), columnString(s.stmt, 2)};
}

std::vector<std::string> RoomDatabase::roomNames() {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> out;
    if (!db) return out;
    Statement s(db, "SELECT name FROM rooms ORDER BY name");
    if (!s) return out;
    while (sqlite3_step(s.stmt) == SQLITE_ROW) out.push_back(columnString(s.stmt, 0));
    return out;
}

bool RoomDatabase::checkPassword(const std::string& name, const std::string& authTokenBase64) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!db) return false;
    Statement s(db, "SELECT password_salt, password_verifier FROM rooms WHERE name=?");
    if (!s) return false;
    sqlite3_bind_text(s.stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s.stmt) != SQLITE_ROW ||
        sqlite3_column_bytes(s.stmt, 0) != kSaltLen ||
        sqlite3_column_bytes(s.stmt, 1) != kVerifierLen) return false;
    const auto* salt = static_cast<const unsigned char*>(sqlite3_column_blob(s.stmt, 0));
    const auto* expected = static_cast<const unsigned char*>(sqlite3_column_blob(s.stmt, 1));
    std::array<unsigned char, kVerifierLen> verifier{};
    if (!makeVerifier(authTokenBase64, salt, verifier)) return false;
    return CRYPTO_memcmp(verifier.data(), expected, kVerifierLen) == 0;
}

bool RoomDatabase::recordMembership(const std::string& name, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!db) return false;
    Statement s(db, "INSERT OR IGNORE INTO room_members(room_name,username) VALUES(?,?)");
    if (!s) return false;
    sqlite3_bind_text(s.stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(s.stmt) == SQLITE_DONE;
}

bool RoomDatabase::saveMessage(const std::string& name, const std::string& sender,
                               const std::string& encryptedBase64) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!db) return false;
    Statement s(db, "INSERT INTO room_messages(room_name,sender,ciphertext_base64) VALUES(?,?,?)");
    if (!s) return false;
    sqlite3_bind_text(s.stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 2, sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s.stmt, 3, encryptedBase64.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(s.stmt) == SQLITE_DONE;
}

std::vector<std::string> RoomDatabase::recentMessages(const std::string& name, int limit) {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> reverse;
    if (!db) return reverse;
    Statement s(db, "SELECT ciphertext_base64 FROM room_messages WHERE room_name=? ORDER BY id DESC LIMIT ?");
    if (!s) return reverse;
    sqlite3_bind_text(s.stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s.stmt, 2, limit);
    while (sqlite3_step(s.stmt) == SQLITE_ROW) reverse.push_back(columnString(s.stmt, 0));
    return {reverse.rbegin(), reverse.rend()};
}

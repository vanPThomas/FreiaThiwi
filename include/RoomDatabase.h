#pragma once

#include <sqlite3.h>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct StoredRoom {
    std::string name;
    std::string creator;
    std::string createdAt;
};

// Persistent room metadata and encrypted message history. Online sessions are
// deliberately NOT persisted: they are rebuilt from live TCP connections.
class RoomDatabase {
public:
    explicit RoomDatabase(const std::string& path = "accounts.db");
    ~RoomDatabase();

    bool ready() const { return db != nullptr; }
    bool createRoom(const std::string& name, const std::string& creator,
                    const std::string& authTokenBase64);
    std::optional<StoredRoom> findRoom(const std::string& name);
    std::vector<std::string> roomNames();
    bool checkPassword(const std::string& name, const std::string& authTokenBase64);
    bool recordMembership(const std::string& name, const std::string& username);
    bool saveMessage(const std::string& name, const std::string& sender,
                     const std::string& encryptedBase64);
    std::vector<std::string> recentMessages(const std::string& name, int limit = 100);

private:
    sqlite3* db = nullptr;
    std::mutex mutex;
    bool initialize();
};

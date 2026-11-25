#pragma once
#include <string>
#include <vector>
#include "FreiaEncryption.h"

class ChatRoom
{
public:
    ChatRoom(std::string chatRoomName, std::vector<std::string> chatMessages);
    ChatRoom(std::string chatRoomName, std::string chatRoomPassword);

    // ======================================
    // GETTERS
    // ======================================

    const std::string getChatRoomName() const { return chatRoomName; }
    const std::vector<std::string> getChatRoomMessages() const { return chatMessages; }
    const std::string getChatRoomPassword() const { return password; }
    const std::vector<std::string> getConnectedUsers() const { return connectedUsers; }
    const std::string getRoomCreator() const { return roomCreator; }
    const std::string getRoomCreationTime() const {return roomCreationTime; }
    
    void addMessage(std::string message);
    void addConnectedUser(std::string user);
    void removeConnectedUser(const std::string& user);
    void setConnectedUsers(std::vector<std::string> users);
    void setMetadata(std::string creator, std::string creationTime);

private:
    std::string chatRoomName;
    std::string password;
    std::vector<std::string> chatMessages;
    std::vector<std::string> connectedUsers;
    std::string roomCreator;
    std::string roomCreationTime;

    FreiaEncryption::Key roomKey{};
};
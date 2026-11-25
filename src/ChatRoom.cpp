#include "ChatRoom.h"

#include <utility>
#include <algorithm>

ChatRoom::ChatRoom(std::string chatRoomName, std::vector<std::string> chatMessages)
    : chatRoomName(chatRoomName)
    , chatMessages(chatMessages)
{
}

ChatRoom::ChatRoom(std::string chatRoomName, std::string chatRoomPassword)
    : chatRoomName(chatRoomName)
    , password(chatRoomPassword)
{
}

void ChatRoom::addMessage(std::string message)
{
    chatMessages.push_back(message);
}

void ChatRoom::addConnectedUser(std::string user)
{
    if (std::find(connectedUsers.begin(), connectedUsers.end(), user) == connectedUsers.end())
        connectedUsers.push_back(std::move(user));
}

void ChatRoom::removeConnectedUser(const std::string& user)
{
    connectedUsers.erase(
        std::remove(connectedUsers.begin(), connectedUsers.end(), user),
        connectedUsers.end());
}
void ChatRoom::setConnectedUsers(std::vector<std::string> users)
{
    connectedUsers = std::move(users);
}

void ChatRoom::setMetadata(std::string creator, std::string creationTime)
{
    roomCreator = std::move(creator);
    roomCreationTime = std::move(creationTime);
}

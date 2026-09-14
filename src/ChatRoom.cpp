#include "ChatRoom.h"

#include <utility>

ChatRoom::ChatRoom(std::string chatRoomName, std::vector<std::string> chatMessages)
    : chatRoomName(chatRoomName)
    , chatMessages(chatMessages)
{
}

ChatRoom::ChatRoom(std::string chatRoomName, std::string chatRoomPassword)
    : chatRoomName(chatRoomName)
    , password(chatRoomPassword)
{
    FreiaEncryption::deriveKey(chatRoomPassword);
}

void ChatRoom::addMessage(std::string message)
{
    chatMessages.push_back(message);
}

void ChatRoom::addConnectedUser(std::string user)
{
    connectedUsers.push_back(user);
}
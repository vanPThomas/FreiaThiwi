#pragma once
#include <string>
#include <iostream>
#include <unistd.h>        // for close()
#include <sys/socket.h>    // for socket(), bind(), listen()
#include <netinet/in.h>    // for sockaddr_in, INADDR_ANY
#include <arpa/inet.h>     // for htons()
#include <cstring>         // for memset, etc.
#include <cstdlib>
#include <sys/types.h>
#include <vector>
#include <sstream>
#include <string>
#include "FreiaEncryption.h"
#include <mutex>
#include <unordered_map>
#include "AccountDatabase.h"
#include "ChatRoom.h"
#include "RoomDatabase.h"
#include <unordered_set>
#include <algorithm>

class Server
{
public:
    Server(int port, int maxClients, const std::string& password);
    void run();

private:
    void handleSystemCallError(std::string errorMsg);
    int initializeServerSocket();
    void waitForServerActivity();
    std::vector<std::string> splitByNewline(const std::string& s);
    bool sendWithLengthPrefix(int sock, const std::string& data);


    // ====================================
    // Protocol processing and creation
    // ====================================

    void processProt1(int clientIndex, const std::string& encrypted, const std::string& plaintext);
    void broadcastProt3(const std::string& messageText, const std::string& messageType, int onlyTo = -1); // -1 is broadcast to all 
    void processProt4(int clientIndex, const std::string& plaintext);
    void sendSuccess(int sock, const std::string& msg);
    void sendError(int sock, const std::string& reason);
    void processProt5(int clientIndex, const std::string& plaintext);
    void sendRoom(int socket, const ChatRoom& room);
    void sendProt5Error(int socket, const std::string& room, const std::string& reason);
    void joinRoom(int socket, const std::string& name, const std::string& username,
                  const std::string& authTokenBase64);
    void leaveRoom(int socket, const std::string& name);
    void refreshRoomUsers(const std::string& name);
    static bool validProtocolName(const std::string& s, size_t maxSize);
    static bool validBase64(const std::string& s, size_t maxSize);
    bool sendEncrypted(int socket, const std::string& frame);

    // ====================================
    // Client activity
    // ====================================

    void connectNewClientSocket();
    void handleClientActivity();
    void sendFullUserList(int targetSocket);
    void sendFullRoomList(int targetSocket);
    void disconnectClient(int index, const std::string& reason = "Unknown");
    void closeClientSocket(int index);
    void collectActiveClientSockets();

    void broadcastRoomUsers(const ChatRoom& room);

    int maxClients;
    int PORT;
    FreiaEncryption::Key serverKey;
    std::string serverPassword;
    std::vector<int> clientSocket;
    int newSocket = -1;
    int valread = 0;
    int currentSocket = -1;
    int activity = 0;
    int max_socket = -1;
    fd_set readfds;
    int addrlen = 0;
    sockaddr_in address{};
    static constexpr int MAX_PACKET_SIZE = 64 * 1024;
    char buffer[MAX_PACKET_SIZE];
    int masterSocket = -1;

    std::unordered_map<int, std::string> socketToUsername;
    std::vector<ChatRoom> roomsWithConnections;
    std::unordered_map<std::string, std::unordered_set<int>> roomSockets;
    std::unordered_map<int, std::string> handshakeUsernames;

    std::mutex socketMutex;

    AccountDatabase accountsDb;
    RoomDatabase roomsDb;



};

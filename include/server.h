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

class Server {
public:
    Server(int port, int maxClients, const std::string& password);
    void run();

private:
    void handleSystemCallError(std::string errorMsg);
    int initializeServerSocket();
    void closeClientSocket(int index);
    void collectActiveClientSockets();
    void waitForServerActivity();
    void connectNewClientSocket();
    void handleClientActivity();
    std::vector<std::string> splitByNewline(const std::string& s);
    void processProt1(int clientIndex, const std::string& encrypted, const std::string& plaintext);
    void disconnectClient(int index, const std::string& reason = "Unknown");


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
    static constexpr int MAX_PACKET_SIZE = 1024;
    char buffer[MAX_PACKET_SIZE];
    int masterSocket = -1;

    std::mutex socketMutex;
};

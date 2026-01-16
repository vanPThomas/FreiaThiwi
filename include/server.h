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
#include "encrypt.h"

class Server {
public:
    Server(int port, int maxClients);
    void run();

private:
    void handleSystemCallError(std::string errorMsg);
    int initializeServerSocket();
    void closeClientSocket(int index);
    void collectActiveClientSockets();
    void waitForServerActivity();
    void connectNewClientSocket();
    void handleClientActivity();


    int maxClients;
    int PORT;
    std::vector<int> clientSocket;   // was int clientSocket[maxClients];
    int newSocket = -1;
    int valread = 0;
    int sd = -1;
    int activity = 0;
    int max_sd = -1;
    fd_set readfds;
    int addrlen = 0;
    sockaddr_in address{};
    static constexpr int bufferSize = 1024;
    char buffer[bufferSize];
    int masterSocket = -1;
};

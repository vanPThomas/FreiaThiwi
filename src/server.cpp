#include "server.h"

Server::Server(int port, int maxClients)
    : maxClients(maxClients), PORT(port) {
    masterSocket = initializeServerSocket();
    clientSocket.assign(maxClients, 0);
    addrlen = sizeof(address);
    std::cout << "Waiting for connections ... \n";
}

void Server::handleSystemCallError(std::string errorMsg)
{
    std::cerr << "Server error on port " << PORT
              << ": " << errorMsg << " (errno=" << errno << ")\n";
    exit(EXIT_FAILURE);
}

int Server::initializeServerSocket() 
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) handleSystemCallError("Failed to create socket");

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        handleSystemCallError("Failed to setsockopt");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    addrlen = sizeof(address);

    if (bind(serverSocket, (struct sockaddr*)&address, addrlen) < 0)
        handleSystemCallError("Failed to bind to port");

    std::cout << "Listening on port " << PORT << "\n";
    if (listen(serverSocket, std::max(1, maxClients)) < 0)
        handleSystemCallError("Failed to listen on socket");

    return serverSocket;
}

void Server::closeClientSocket(int index)
{
    close(clientSocket[index]);
    clientSocket[index] = 0;
}

void Server::collectActiveClientSockets()
{
    for (int i = 0; i < maxClients; i++)
    {
        sd = clientSocket[i];

        // if valid socket, add to set
        if (sd > 0)
            FD_SET(sd, &readfds);
        // highest file descriptor number, needed for select func
        if (sd > max_sd)
            max_sd = sd;
    }
}

void Server::waitForServerActivity()
{
    // wait indeffinitely for socket activity (timeout is NULL)
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR))
    {
        std::cout << "Select error\n";
    }
}

void Server::connectNewClientSocket()
{
    if (FD_ISSET(masterSocket, &readfds))
    {
        newSocket = accept(masterSocket, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        std::cout << "New connection , socket fd is " << newSocket << " , ip is " << inet_ntoa(address.sin_addr) << " , port : " << ntohs(address.sin_port) << "\n";

        for (int i = 0; i < maxClients; i++)
        {
            if (clientSocket[i] == 0)
            {
                clientSocket[i] = newSocket;
                std::cout << "Adding to list of sockets as " << i << "\n";
                break;
            }
        }
    }
}

void Server::handleClientActivity()
{
    for (int i = 0; i < maxClients; i++)
    {
        sd = clientSocket[i];

        if (FD_ISSET(sd, &readfds))
        {
            uint32_t msgLength = 0;

            // --- read the 4-byte length first ---
            int lenRead = recv(sd, &msgLength, sizeof(msgLength), MSG_WAITALL);
            if (lenRead <= 0)
            {
                getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                std::cout << "Host Disconnected! ip: " << inet_ntoa(address.sin_addr)
                          << " port: " << ntohs(address.sin_port) << "\n";
                closeClientSocket(i);
                continue;
            }

            msgLength = ntohl(msgLength); // convert from network byte order

            if (msgLength == 0 || msgLength > bufferSize)
            {
                std::cout << "[Warning] Invalid message length: " << msgLength << "\n";
                closeClientSocket(i);
                continue;
            }

            // --- read message payload ---
            std::vector<char> message(msgLength);
            int msgRead = recv(sd, message.data(), msgLength, MSG_WAITALL);
            if (msgRead <= 0)
            {
                std::cout << "[Error] Failed to receive full payload.\n";
                closeClientSocket(i);
                continue;
            }

            // Print message to console
            std::cout.write(message.data(), msgLength);
            std::cout << "\n";

            // --- broadcast to other clients (same framing!) ---
            uint32_t netLength = htonl(msgLength);
            for (int j = 0; j < maxClients; ++j)
            {
                int sdTarget = clientSocket[j];
                if (sdTarget != 0 && sdTarget != sd)
                {
                    send(sdTarget, &netLength, sizeof(netLength), 0);
                    send(sdTarget, message.data(), msgLength, 0);
                }
            }
        }
    }
}



void Server::run()
{
    while (true)
    {
        // clear socket set
        FD_ZERO(&readfds);

        // add mastersocket to socket set
        FD_SET(masterSocket, &readfds);
        max_sd = masterSocket;
        
        collectActiveClientSockets();
        waitForServerActivity();
        connectNewClientSocket();
        handleClientActivity();
    }
}
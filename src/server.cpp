#include "server.h"

Server::Server(int port, int maxClients, const std::string& password)
    : maxClients(maxClients), PORT(port), serverPassword(password) {
        serverKey = FreiaEncryption::deriveKey(serverPassword);
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
    std::lock_guard<std::mutex> lock(socketMutex);
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
    std::lock_guard<std::mutex> lock(socketMutex);
    for (int i = 0; i < maxClients; i++)
    {
        currentSocket = clientSocket[i];

        // if valid socket, add to set
        if (currentSocket > 0)
            FD_SET(currentSocket, &readfds);
        // highest file descriptor number, needed for select func
        if (currentSocket > max_socket)
            max_socket = currentSocket;
    }
}

void Server::waitForServerActivity()
{
    // wait indeffinitely for socket activity (timeout is NULL)
    activity = select(max_socket + 1, &readfds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR))
    {
        handleSystemCallError("Select error\n");
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
            std::lock_guard<std::mutex> lock(socketMutex);
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
        std::lock_guard<std::mutex> lock(socketMutex);
        currentSocket = clientSocket[i];
        if (currentSocket == 0 || !FD_ISSET(currentSocket, &readfds))
            continue;

        uint32_t packetLengthNet = 0;
        int r = recv(currentSocket, &packetLengthNet, sizeof(packetLengthNet), MSG_WAITALL);
        if (r <= 0)
        {
            getpeername(currentSocket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            std::cout << "Host disconnected! ip: " << inet_ntoa(address.sin_addr)
                      << " port: " << ntohs(address.sin_port) << "\n";
            closeClientSocket(i);
            continue;
        }

        uint32_t packetLength = ntohl(packetLengthNet);
        if (packetLength == 0 || packetLength > MAX_PACKET_SIZE)
        {
            std::string errWarning = "[Warning] Invalid length: " + packetLength + std::string("\n");
            disconnectClient(i, errWarning);
            continue;
        }

        std::string encrypted(packetLength, '\0');
        r = recv(currentSocket, encrypted.data(), packetLength, MSG_WAITALL);
        if (r <= 0)
        {
            disconnectClient(i, "[Error] Failed to read payload\n");
            continue;
        }

        // Decrypt with server password key
        std::string plaintext = FreiaEncryption::decryptData(encrypted, serverKey);
        if (plaintext.empty())
        {
            disconnectClient(i, "[Auth fail] Decryption failed - likely wrong server password\n");
            continue;
        }

        auto parts = splitByNewline(plaintext);
        std::string protocol = parts[0];
        if(protocol == "PROT1")
        {
            processProt1(i, encrypted, plaintext);
        }
        else
        {
            // std::cout << "[Protocol error] Malformed or missing Protocol1\n";
            handleSystemCallError("[Protocol error] Malformed or missing Protocol1\n");
        }
    }
}

void Server::processProt1(int clientIndex, const std::string& encrypted, const std::string& plaintext)
{
    std::cout << "test\n";

    int currentSocket = clientSocket[clientIndex];

    auto parts = splitByNewline(plaintext);

    std::string username = parts[1];
    size_t innerLen = 0;
    try {
        innerLen = std::stoul(parts[2]);
    } catch (...) {
        disconnectClient(clientIndex, "[Protocol error] Invalid length field\n");
        return;
    }
    
    if (innerLen == 0 || innerLen > plaintext.size())
    {
        handleSystemCallError("[Protocol error] Inner length out of range\n");
        closeClientSocket(clientIndex);

        return;
    }

    std::string innerCipher = plaintext.substr(plaintext.size() - innerLen);

    // Success! Log
    std::cout << "[PROT1] From user '" << username << "' - inner ciphertext size: "
              << innerLen << " bytes\n";

    // Forward the original encrypted packet to all other clients
    uint32_t packetLengthNet = htonl(encrypted.size());  // original encrypted length
    for (int j = 0; j < maxClients; ++j)
    {
        int socketTarget = clientSocket[j];
        if (socketTarget != 0 && socketTarget != currentSocket)
        {

            ssize_t s1 = send(socketTarget, &packetLengthNet, sizeof(packetLengthNet), 0);
            ssize_t s2 = send(socketTarget, encrypted.data(), encrypted.size(), 0);

            if (s1 != sizeof(packetLengthNet) || s2 != static_cast<ssize_t>(encrypted.size()))
            {
                std::string errWarning = "[Warning] Failed to forward to socket " + socketTarget + std::string("\n");
                handleSystemCallError(errWarning);
            }
            else
            {
                std::cout << "[Forwarded] " << encrypted.size() << " bytes to socket " << socketTarget << "\n";
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
        max_socket = masterSocket;
        
        collectActiveClientSockets();
        waitForServerActivity();
        connectNewClientSocket();
        handleClientActivity();
    }
}

std::vector<std::string> Server::splitByNewline(const std::string& s)
{
    std::vector<std::string> lines;
    std::string line;
    std::istringstream iss(s);
    while (std::getline(iss, line)) {
        if (!line.empty() || !lines.empty()) {
            lines.push_back(std::move(line));
        }
    }
    return lines;
}

void Server::disconnectClient(int index, const std::string& reason)
{
    getpeername(clientSocket[index], (struct sockaddr*)&address, (socklen_t*)&addrlen);
    std::cerr << "Client disconnected (" << reason << "): "
              << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << "\n";
    closeClientSocket(index);
}
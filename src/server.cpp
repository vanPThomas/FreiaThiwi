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
        if (sd == 0 || !FD_ISSET(sd, &readfds))
            continue;

        uint32_t netLen = 0;
        int r = recv(sd, &netLen, sizeof(netLen), MSG_WAITALL);
        if (r <= 0)
        {
            getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            std::cout << "Host disconnected! ip: " << inet_ntoa(address.sin_addr)
                      << " port: " << ntohs(address.sin_port) << "\n";
            closeClientSocket(i);
            continue;
        }

        uint32_t len = ntohl(netLen);
        if (len == 0 || len > bufferSize)
        {
            std::cout << "[Warning] Invalid length: " << len << "\n";
            closeClientSocket(i);
            continue;
        }

        std::string encrypted(len, '\0');
        r = recv(sd, encrypted.data(), len, MSG_WAITALL);
        if (r <= 0)
        {
            std::cout << "[Error] Failed to read payload\n";
            closeClientSocket(i);
            continue;
        }

        // Decrypt with server password key
        std::string plaintext = FreiaEncryption::decryptData(encrypted, serverKey);
        if (plaintext.empty())
        {
            std::cout << "[Auth fail] Decryption failed - likely wrong server password\n";
            closeClientSocket(i);
            continue;
        }

        auto parts = splitByNewline(plaintext);
        if (parts.empty() || parts.size() < 3)
        {
            std::cout << "[Protocol error] Malformed Protocol\n";
            closeClientSocket(i);
            continue;
        }
        
        std::string protocol = parts [0];
        if(protocol == "PROT1")
        {
            std::string username = parts[1];
            size_t innerLen = 0;
            try {
                innerLen = std::stoul(parts[2]);
            } catch (...) {
                std::cout << "[Protocol error] Invalid length field\n";
                closeClientSocket(i);
                continue;
            }
    
            if (innerLen == 0 || innerLen > plaintext.size())
            {
                std::cout << "[Protocol error] Inner length out of range\n";
                closeClientSocket(i);
                continue;
            }
    
            std::string innerCipher = plaintext.substr(plaintext.size() - innerLen);
    
            // Success! Log what we got
            std::cout << "[PROT1] From user '" << username << "' - inner ciphertext size: " << innerLen << " bytes\n";
            
            // send to all connected
            for (int j = 0; j < maxClients; ++j)
            {
                int sdTarget = clientSocket[j];
                if (sdTarget != 0 && sdTarget != sd)
                {
                    ssize_t s1 = send(sdTarget, &netLen, sizeof(netLen), 0);
                    ssize_t s2 = send(sdTarget, encrypted.data(), len, 0);

                    if (s1 != sizeof(netLen) || s2 != static_cast<ssize_t>(len))
                    {
                        std::cout << "[Warning] Failed to forward to socket " << sdTarget << "\n";
                    }
                    else
                    {
                        std::cout << "[Forwarded] " << len << " bytes to socket " << sdTarget << "\n";
                    }
                }
            }
        }
        else
        {
            std::cout << "[Unknown protocol] " << parts[0] << " - closing connection\n";
            closeClientSocket(i);
            continue;
        }

        // TODO later: store username -> sd mapping
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

std::vector<std::string> Server::splitByNewline(const std::string& s) {
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
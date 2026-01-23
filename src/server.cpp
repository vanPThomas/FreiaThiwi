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
        int newSocket = accept(masterSocket, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (newSocket < 0) {
            handleSystemCallError("accept failed");
            return;
        }

        std::string clientIp = inet_ntoa(address.sin_addr);
        int clientPort = ntohs(address.sin_port);
        std::cout << "New incoming connection: " << clientIp << ":" << clientPort << " (fd=" << newSocket << ")\n";        
        
        // ────────────────────────────────────────────────
        //  HANDSHAKE / AUTHENTICATION RIGHT HERE
        // ────────────────────────────────────────────────

        // 1. Read length prefix
        uint32_t lenNet = 0;
        int r = recv(newSocket, &lenNet, sizeof(lenNet), MSG_WAITALL);
        if (r != sizeof(lenNet)) {
            std::cout << "Handshake failed: incomplete length prefix from " 
                    << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        uint32_t len = ntohl(lenNet);
        if (len == 0 || len > 65536) {
            std::cout << "Handshake failed: invalid length " << len 
                    << " from " << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        // 2. Read ciphertext
        std::string cipher(len, '\0');
        r = recv(newSocket, cipher.data(), len, MSG_WAITALL);
        if (r != static_cast<int>(len)) {
            std::cout << "Handshake failed: incomplete payload from " 
                    << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        // 3. Decrypt
        std::string plain = FreiaEncryption::decryptData(cipher, serverKey);
        if (plain.empty()) {
            std::cout << "Handshake failed: decryption failed (wrong password?) from " 
                    << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        // 4. Parse PROT2 handshake
        auto parts = splitByNewline(plain);
        if (parts.size() < 2 || parts[0] != "PROT2") {
            std::cout << "Handshake failed: invalid format from " 
                    << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        std::string username = parts[1];
        // Optional: validate username (length, chars, sanitize)
        if (username.empty() || username.size() > 64) {
            std::cout << "Handshake failed: invalid username length from " 
                    << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        // ────────────────────────────────────────────────
        // SUCCESS: authenticated & username known
        // ────────────────────────────────────────────────

        // Store username immediately
        {
            std::lock_guard<std::mutex> lock(socketMutex);
            socketToUsername[newSocket] = username;
        }

        std::cout << "Authenticated: " << username << " from " 
                << clientIp << ":" << clientPort << " (fd=" << newSocket << ")\n";

        // 5. Send OK reply (encrypted)
        std::string okPlain = "PROT2\nWelcome " + username + "!";
        std::string okCipher = FreiaEncryption::encryptData(okPlain, serverKey);
        if (okCipher.empty()) {
            std::cerr << "[Critical] Failed to encrypt PROT2 reply\n";
            close(newSocket);
            socketToUsername.erase(newSocket);
            return;
        }

        uint32_t okLenNet = htonl(okCipher.size());
        if (send(newSocket, &okLenNet, sizeof(okLenNet), 0) != sizeof(okLenNet) ||
            send(newSocket, okCipher.data(), okCipher.size(), 0) != static_cast<ssize_t>(okCipher.size())) {
            std::cout << "Failed to send OK reply to " << username << "\n";
            close(newSocket);
            socketToUsername.erase(newSocket);
            return;
        }
                
        bool added = false;
        {
            std::lock_guard<std::mutex> lock(socketMutex);
            for (int i = 0; i < maxClients; ++i) {
                if (clientSocket[i] == 0) {
                    clientSocket[i] = newSocket;
                    std::cout << "Added authenticated client " << username 
                            << " at slot " << i << "\n";
                    added = true;
                    break;
                }
            }

        }
        if (!added) {
            std::cout << "Server full - rejecting " << username << "\n";
            close(newSocket);
            socketToUsername.erase(newSocket);
        }
    }
}

void Server::handleClientActivity()
{
    std::lock_guard<std::mutex> lock(socketMutex);
    for (int i = 0; i < maxClients; i++)
    {
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
            disconnectClient(i, "Client Disconnected");
            // closeClientSocket(i);
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

void Server::broadcastProt3(const std::string& messageText, const std::string& messageType)
{

    std::string frame = "PROT3\n" + messageType + "\n" + messageText;

    std::string encrypted = FreiaEncryption::encryptData(frame, serverKey);
    if (encrypted.empty()) {
        std::cerr << "[Error] Failed to encrypt PROT3 message\n";
        return;
    }

    uint32_t len = encrypted.size();
    uint32_t netLen = htonl(len);

    //std::lock_guard<std::mutex> lock(socketMutex);

    for (int j = 0; j < maxClients; ++j) {
        int target = clientSocket[j];
        if (target <= 0) continue;

        // Optional: skip the disconnecting client if you have its fd available
        // if (target == currentSocket) continue;  // but currentSocket not in scope here

        ssize_t s1 = send(target, &netLen, sizeof(netLen), 0);
        ssize_t s2 = send(target, encrypted.data(), encrypted.size(), 0);
        std::cout << "TESTTEST!\n";

        if (s1 != sizeof(netLen) || s2 != static_cast<ssize_t>(encrypted.size())) {
            std::cerr << "[Warning] Failed to send PROT3 to socket " << target << "\n";
            // You could close this socket here if you want aggressive cleanup
        } else {
            // Optional debug log
            std::cout << "[Sent PROT3] " << messageType << " (" << len << " bytes) to " << target << "\n";

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
    int victimFd = clientSocket[index];           // remember before we zero it

    std::string username = "Unknown";
    {
        // std::lock_guard<std::mutex> lock(socketMutex);
        auto it = socketToUsername.find(victimFd);
        if (it != socketToUsername.end()) {
            username = it->second;
            socketToUsername.erase(it);
        }
    }

    std::string message = username + " disconnected.";
    std::string messageType = "userDisconnected";

    // Close & clear **first**
    closeClientSocket(index);   // now clientSocket[index] = 0 and fd closed

    // Now broadcast — loop will skip this slot because == 0
    broadcastProt3(message, messageType);

    // Log last
    getpeername(victimFd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    std::cerr << "Client disconnected (" << reason << "): "
              << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port)
              << " (" << username << ")\n";
}
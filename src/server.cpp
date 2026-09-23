#include "server.h"


// Server constructor
Server::Server(int port, int maxClients, const std::string& password)
    : maxClients(maxClients), PORT(port), serverPassword(password), accountsDb("accounts.db")
    {
        serverKey = FreiaEncryption::deriveKey(serverPassword);
        masterSocket = initializeServerSocket();
        clientSocket.assign(maxClients, 0);
        addrlen = sizeof(address);
        std::cout << "Waiting for connections ... \n";

        ChatRoom room1("Test", "Test");
        ChatRoom room2("Test2", "Test2");
        
        std::string room1name = room1.getChatRoomName();
        std::string room2name = room2.getChatRoomName();
        onlineRooms.push_back(room1name);
        onlineRooms.push_back(room2name);

        fakeDatabaseRooms.push_back(room1);
        fakeDatabaseRooms.push_back(room2);
}

// System error fuction
void Server::handleSystemCallError(std::string errorMsg)
{
    std::cerr << "Server error on port " << PORT
              << ": " << errorMsg << " (errno=" << errno << ")\n";
    exit(EXIT_FAILURE);
}

// Create server socket
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

// wait indeffinitely for socket activity (timeout is NULL)
void Server::waitForServerActivity()
{
    activity = select(max_socket + 1, &readfds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR))
    {
        handleSystemCallError("Select error\n");
    }
}

// Main server loop
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

// Split a string based on new line marker
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

// Send Package
bool Server::sendWithLengthPrefix(int sock, const std::string& data)
{
    if (sock <= 0) return false;
    uint32_t lenNet = htonl(static_cast<uint32_t>(data.size()));
    if (send(sock, &lenNet, sizeof(lenNet), 0) != sizeof(lenNet)) return false;
    if (send(sock, data.data(), data.size(), 0) != static_cast<ssize_t>(data.size())) return false;
    return true;
}

// ====================================
// Protocol processing and creation
// ====================================

// Process Protocol when One
void Server::processProt1(int clientIndex, const std::string& encrypted, const std::string& plaintext)
{
    int currentSocket = clientSocket[clientIndex];

    auto parts = splitByNewline(plaintext);

    std::string username = parts[1];
    size_t innerLen = 0;
    try
    {
        innerLen = std::stoul(parts[2]);
    } catch (...)
    {
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

    std::cout << "[PROT1] From user '" << username << "' - inner ciphertext size: "
              << innerLen << " bytes\n";

    for (int j = 0; j < maxClients; ++j)
    {
        int socketTarget = clientSocket[j];
        if (socketTarget != 0 && socketTarget != currentSocket)
        {
            if(!sendWithLengthPrefix(socketTarget, encrypted))
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

// Process Protocol when 3
void Server::broadcastProt3(const std::string& messageText, const std::string& messageType, int onlyTo)
{
    std::string frame = "PROT3\n" + messageType + "\n" + messageText;
    std::string encrypted = FreiaEncryption::encryptData(frame, serverKey);
    if (encrypted.empty()) return;
    
    if (onlyTo == -1)
    {
        for (int j = 0; j < maxClients; ++j)
        {
            int target = clientSocket[j];
            if (onlyTo != -1 && target != onlyTo) continue;
            sendWithLengthPrefix(target, encrypted);            
        }
    }
    else
    {
        for (int j = 0; j < maxClients; ++j)
        {
            int target = clientSocket[j];
            // if (target <= 0) continue;

            if (target == onlyTo)
            {
                sendWithLengthPrefix(target, encrypted);
            }
        }
    } 
}

// Process Protocol when 4, account creation or login
void Server::processProt4(int clientIndex, const std::string& plaintext)
{
    int sock = clientSocket[clientIndex];

    auto parts = splitByNewline(plaintext);
    if (parts.size() < 3)
    {
        sendError(sock, "Malformed PROT4");
        return;
    }

    std::string cmd = parts[1];
    std::string username = parts[2];
    std::string receivedKeyB64 = (parts.size() > 3) ? parts[3] : "";

    // Basic validation
    if (username.empty() || username.size() > 64 || receivedKeyB64.empty())
    {
        sendError(sock, "Invalid username or key");
        return;
    }

    if (cmd == "CREATE")
    {
        if (accountsDb.createAccount(username, receivedKeyB64))
        {
            std::cout << "[Account created] " << username << "\n";
            sendSuccess(sock, "Account created successfully");
        } else
        {
            sendError(sock, "Username already taken or creation failed");
        }
    } else if (cmd == "LOGIN")
    {
        if (accountsDb.validateLogin(username, receivedKeyB64))
        {
            std::cout << "[Login success] " << username << "\n";
            sendSuccess(sock, "Login successful");
        } else
        {
            sendError(sock, "Username not found or incorrect key");
        }
    }
    else
    {
        sendError(sock, "Unknown PROT4 command");
    }
}

// Send Success Message using Protocol 4
void Server::sendSuccess(int sock, const std::string& msg = "")
{
    std::string frame = "PROT4\nSUCCESS";
    if (!msg.empty()) frame += "\n" + msg;

    std::string enc = FreiaEncryption::encryptData(frame, serverKey);
    if (enc.empty()) return;

    sendWithLengthPrefix(sock, enc);
}

// Send error message using protocol 4
void Server::sendError(int sock, const std::string& reason)
{
    std::string frame = "PROT4\nFAIL\n" + reason;

    std::string enc = FreiaEncryption::encryptData(frame, serverKey);
    if (enc.empty()) return;

    sendWithLengthPrefix(sock, enc);
}

// Process prot 5, room creation or login 
void Server::processProt5(int clientIndex, const std::string& plaintext)
{
    int sock = clientSocket[clientIndex];
    auto parts = splitByNewline(plaintext);

    std::string cmd = parts[1];
    std::string chatRoomName = parts[2];
    std::string username = parts[3];
    std::string receivedKeyB64 = (parts.size() > 4) ? parts[4] : "";

    bool foundRoom = false;
    
    if (cmd == "CREATE")
    {
        ChatRoom newRoom(chatRoomName, receivedKeyB64);
        fakeDatabaseRooms.push_back(newRoom);
        onlineRooms.push_back(newRoom.getChatRoomName());
        std::cout << "New Room Created\n";
        
        for (const auto& [fd, name] : socketToUsername)
        {
            sendFullRoomList(fd);
        }
    }
    else if (cmd == "CONNECT")
    {
        std::cout << "AAAAA\n";
        // TODO: password check added when database is added
        for (auto room : roomsWithConnections)
        {
            if (room.getChatRoomName() == chatRoomName)
            {
                room.addConnectedUser(username);
                broadcastProt3("Connected to Room", "SUCCESS", sock);
                foundRoom = true;
                return;
            }
        }
        for (auto room : onlineRooms)
        {
            if (room == chatRoomName)
            {
                for (auto roomdb : fakeDatabaseRooms)
                {
                    if (roomdb.getChatRoomName() == chatRoomName)
                    {
                        roomdb.addConnectedUser(username);
                        roomsWithConnections.push_back(roomdb);
                        broadcastProt3("Connected to Room", "SUCCESS", sock);
                        sendRoom(sock, roomdb);
                        foundRoom = true;
                        return;
                    }
                }
            }
        }

        std::cout << "BBBBBBBBB\n";
    }
    else if (cmd == "MESSAGE")
    {
        auto parts = splitByNewline(plaintext);
        std::string chatRoomName = parts[2];

        size_t innerLen = 0;
        try
        {
            innerLen = std::stoul(parts[3]);
        } catch (...)
        {
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

        std::cout << "[PROT5] From user '" << chatRoomName << "' - inner ciphertext size: " << innerLen << " bytes\n";

        std::string allProt5Frame;
        allProt5Frame += "PROT5\n";
        allProt5Frame += "MESSAGE\n";
        allProt5Frame += chatRoomName;
        allProt5Frame += '\n';
        allProt5Frame += innerCipher;

        std::string encrypted = FreiaEncryption::encryptData(allProt5Frame, serverKey);

        for (int j = 0; j < maxClients; ++j)
        {
            int target = clientSocket[j];
            sendWithLengthPrefix(target, encrypted);            
        }

        for (auto room : roomsWithConnections)
        {
            room.addMessage(encrypted);
        }
    }
    else
    {
        sendError(sock, "Unknown PROT5 command");
    }

    if (!foundRoom)
    {
        broadcastProt3("FAILED TO CONNECT! Unknown Room!", "FAIL", clientIndex);
    }
}

// Send room information to client
void Server::sendRoom(int clientIndex, ChatRoom room)
{
    std::string frame = "PROT5\n";
    std::string chatRoomName = room.getChatRoomName();
    std::string password = room.getChatRoomPassword();
    std::vector<std::string> chatMessages = room.getChatRoomMessages();
    std::vector<std::string> connectedUsers = room.getConnectedUsers();
    std::string roomCreator = room.getRoomCreator();
    std::string roomCreationTime = room.getRoomCreationTime();

    frame += "ROOM\n" + chatRoomName + "\n" + password + "\n" + "MESSAGES\n";
    for (auto message : chatMessages)
    {
        frame += message + "\n";
    }
    frame += "USERS\n";
    for (auto user : connectedUsers)
    {
        frame += user + "\n";
    }

    frame += "END\n" + roomCreator + "\n" + roomCreationTime + "\n";
    std::string encrypted = FreiaEncryption::encryptData(frame, serverKey);
    sendWithLengthPrefix(clientIndex, encrypted);
}

// ====================================
// Client activity
// ====================================

//Connect new client to server
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
        
        //  HANDSHAKE / AUTHENTICATION

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
        // Validate username (length, chars, sanitize)
        if (username.empty() || username.size() > 64) {
            std::cout << "Handshake failed: invalid username length from " 
                    << clientIp << ":" << clientPort << "\n";
            close(newSocket);
            return;
        }

        // SUCCESS: authenticated & username known

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
        if (okCipher.empty())
        {
            std::cerr << "[Critical] Failed to encrypt PROT2 reply\n";
            close(newSocket);
            socketToUsername.erase(newSocket);
            return;
        }

        if (!sendWithLengthPrefix(newSocket, okCipher))
        {
            std::cout << "Failed to send OK reply to " << username << "\n";
            close(newSocket);
            socketToUsername.erase(newSocket);
            return;
        }

        broadcastProt3(username, "userJoined");
        
        bool added = false;
        {
            std::lock_guard<std::mutex> lock(socketMutex);
            for (int i = 0; i < maxClients; ++i)
            {
                if (clientSocket[i] == 0)
                {
                    clientSocket[i] = newSocket;
                    std::cout << "Added authenticated client " << username 
                    << " at slot " << i << "\n";
                    added = true;
                    break;
                }
            }
            
        }
        if (!added)
        {
            std::cout << "Server full - rejecting " << username << "\n";
            close(newSocket);
            socketToUsername.erase(newSocket);
        }
        sendFullUserList(newSocket);
        sendFullRoomList(newSocket);
    }
}

// Handle any activity from any of the clients
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
        std::cout << protocol << "\n";

        if(protocol == "PROT1")
        {
            processProt1(i, encrypted, plaintext);
        }
        else if (protocol == "PROT4")
        {
            processProt4(i, plaintext);
        }
        else if (protocol == "PROT5")
        {
            processProt5(i, plaintext);
        }
        else
        {
            handleSystemCallError("[Protocol error] Malformed or missing Protocol1\n");
        }
    }
}

// Send full user list to one specific client
void Server::sendFullUserList(int targetSocket)
{
    std::string list;
    {
        for (const auto& [fd, name] : socketToUsername)
        {
            if (!list.empty()) list += "\n";
            list += name;
        }
    }

    if (list.empty()) list = "";
    broadcastProt3(list, "userList", targetSocket);
}

// Send full room list to one specific client
void Server::sendFullRoomList(int targetSocket)
{
    std::string list;
    {
        for (auto name : onlineRooms)
        {
            if (!list.empty()) list += "\n";
            list += name;
        }
    }

    if (list.empty()) list = "";
    broadcastProt3(list, "roomList", targetSocket);
}

// Disconnect client from server
void Server::disconnectClient(int index, const std::string& reason)
{
    int victimFd = clientSocket[index];

    std::string username = "Unknown";
    {
        auto it = socketToUsername.find(victimFd);
        if (it != socketToUsername.end()) {
            username = it->second;
            socketToUsername.erase(it);
        }
    }

    std::string message = username + " disconnected.";
    std::string messageType = "userDisconnected";

    // Close & clear
    closeClientSocket(index);
    
    broadcastProt3(message, messageType);
    broadcastProt3(username, "userLeft");

    // Log last
    getpeername(victimFd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    std::cerr << "Client disconnected (" << reason << "): "
              << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port)
              << " (" << username << ")\n";
}

// close client socket
void Server::closeClientSocket(int index)
{
    close(clientSocket[index]);
    clientSocket[index] = 0;
}

// Collect sockets with active clients
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

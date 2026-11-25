#include "server.h"
#include <cerrno>
#include <cctype>


// Server constructor
Server::Server(int port, int maxClients, const std::string& password)
    : maxClients(maxClients), PORT(port), serverPassword(password),
      accountsDb("accounts.db"), roomsDb("accounts.db")
{
    if (!roomsDb.ready()) handleSystemCallError("Room database initialization failed");
    serverKey = FreiaEncryption::deriveKey(serverPassword);
    masterSocket = initializeServerSocket();
    clientSocket.assign(maxClients, 0);
    addrlen = sizeof(address);
    std::cout << "Loaded " << roomsDb.roomNames().size()
              << " persisted chatrooms; waiting for connections...\n";
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

// Send all bytes, including when send() writes only a prefix.
bool Server::sendWithLengthPrefix(int sock, const std::string& data)
{
    if (sock <= 0 || data.empty() || data.size() > MAX_PACKET_SIZE) return false;
    uint32_t lenNet = htonl(static_cast<uint32_t>(data.size()));
    auto sendAll = [sock](const char* ptr, size_t length) {
        size_t sent = 0;
        while (sent < length) {
            ssize_t n = send(sock, ptr + sent, length - sent, MSG_NOSIGNAL);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    };
    return sendAll(reinterpret_cast<const char*>(&lenNet), sizeof(lenNet)) &&
           sendAll(data.data(), data.size());
}

bool Server::sendEncrypted(int socket, const std::string& frame)
{
    std::string encrypted = FreiaEncryption::encryptData(frame, serverKey);
    return !encrypted.empty() && sendWithLengthPrefix(socket, encrypted);
}

bool Server::validProtocolName(const std::string& value, size_t limit)
{
    if (value.empty() || value.size() > limit || value.front() == ' ' || value.back() == ' ')
        return false;
    for (unsigned char c : value)
        if (c < 32 || c == 127) return false;
    return true;
}

bool Server::validBase64(const std::string& value, size_t maxSize)
{
    if (value.empty() || value.size() > maxSize || value.size() % 4 != 0) return false;
    size_t equals = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '=') {
            if (i < value.size() - 2) return false;
            ++equals;
        } else if (equals || !((c >= 'A' && c <= 'Z') ||
                                (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '+' || c == '/')) {
            return false;
        }
    }
    return equals <= 2;
}

// ====================================
// Protocol processing and creation
// ====================================

// Relay legacy direct messages. Reject invalid frames without killing the server.
void Server::processProt1(int clientIndex, const std::string& encrypted, const std::string& plaintext)
{
    int sender = clientSocket[clientIndex];
    if (socketToUsername.find(sender) == socketToUsername.end()) return;
    auto first = plaintext.find('\n');
    auto second = plaintext.find('\n', first == std::string::npos ? first : first + 1);
    auto third = plaintext.find('\n', second == std::string::npos ? second : second + 1);
    if (second == std::string::npos || third == std::string::npos) return;
    if (plaintext.substr(first + 1, second - first - 1) != socketToUsername[sender]) return;
    size_t len = 0;
    try {
        auto sizeString = plaintext.substr(second + 1, third - second - 1);
        size_t pos = 0;
        len = std::stoul(sizeString, &pos);
        if (pos != sizeString.size()) return;
    } catch (...) { return; }
    if (len == 0 || len != plaintext.size() - third - 1) return;
    for (int target : clientSocket)
        if (target > 0 && target != sender && socketToUsername.count(target))
            sendWithLengthPrefix(target, encrypted);
}

// Protocol 3 server notices.
void Server::broadcastProt3(const std::string& messageText, const std::string& messageType, int onlyTo)
{
    const std::string encrypted = FreiaEncryption::encryptData(
        "PROT3\n" + messageType + "\n" + messageText, serverKey);
    if (encrypted.empty()) return;
    for (int target : clientSocket) {
        if (target <= 0) continue;
        if (onlyTo != -1 && target != onlyTo) continue;
        if (onlyTo == -1 && socketToUsername.count(target) == 0) continue;
        sendWithLengthPrefix(target, encrypted);
    }
}

// Account authentication: the PROT2 username is only a claim, not authentication.
void Server::processProt4(int clientIndex, const std::string& plaintext)
{
    int sock = clientSocket[clientIndex];
    const auto parts = splitByNewline(plaintext);
    if (parts.size() != 4 || !validProtocolName(parts[2], 32) ||
        !validBase64(parts[3], 128)) {
        sendError(sock, "Malformed account request");
        return;
    }
    const std::string& command = parts[1];
    const std::string& username = parts[2];
    if (handshakeUsernames[sock] != username) {
        sendError(sock, "Account username must match handshake");
        return;
    }
    if (socketToUsername.count(sock)) {
        sendError(sock, "Already logged in");
        return;
    }
    bool success = false;
    if (command == "CREATE") success = accountsDb.createAccount(username, parts[3]);
    else if (command == "LOGIN") success = accountsDb.validateLogin(username, parts[3]).has_value();
    else { sendError(sock, "Unknown account command"); return; }
    if (!success) {
        sendError(sock, "Account already exists or credentials invalid");
        return;
    }
    socketToUsername[sock] = username;
    sendSuccess(sock, command == "CREATE" ? "Account created successfully" : "Login successful");
    broadcastProt3(username, "userJoined");
    sendFullUserList(sock);
    sendFullRoomList(sock);
    std::cout << "[Account authenticated] " << username << "\n";
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

// Room-specific errors are distinct from account login errors.
void Server::sendProt5Error(int socket, const std::string& room, const std::string& reason)
{
    sendEncrypted(socket, "PROT5\nERROR\n" + room + "\n" + reason);
}

void Server::joinRoom(int socket, const std::string& name, const std::string& username,
                      const std::string& authTokenBase64)
{
    if (!roomsDb.checkPassword(name, authTokenBase64)) {
        sendProt5Error(socket, name, "Unknown room or incorrect password");
        return;
    }
    if (!roomsDb.recordMembership(name, username)) {
        sendProt5Error(socket, name, "Could not record room membership");
        return;
    }
    auto metadata = roomsDb.findRoom(name);
    if (!metadata) { sendProt5Error(socket, name, "Room not found"); return; }

    auto roomIt = std::find_if(roomsWithConnections.begin(), roomsWithConnections.end(),
        [&](const ChatRoom& room) { return room.getChatRoomName() == name; });
    if (roomIt == roomsWithConnections.end()) {
        roomsWithConnections.emplace_back(name, std::vector<std::string>{});
        roomIt = roomsWithConnections.end() - 1;
        roomIt->setMetadata(metadata->creator, metadata->createdAt);
    }
    roomSockets[name].insert(socket);
    refreshRoomUsers(name);
    // Ordering: ROOM must arrive before USERS or any history MESSAGE.
    sendRoom(socket, *roomIt);
    for (const auto& history : roomsDb.recentMessages(name))
        sendEncrypted(socket, "PROT5\nMESSAGE\n" + name + "\n" + history);
    broadcastRoomUsers(*roomIt);
}

void Server::leaveRoom(int socket, const std::string& name)
{
    auto it = roomSockets.find(name);
    if (it == roomSockets.end() || it->second.erase(socket) == 0) {
        sendProt5Error(socket, name, "Not connected to room");
        return;
    }
    if (it->second.empty()) roomSockets.erase(it);
    // LEFT is sent before users broadcast; only current members receive USERS.
    sendEncrypted(socket, "PROT5\nLEFT\n" + name);
    refreshRoomUsers(name);
    for (const auto& room : roomsWithConnections)
        if (room.getChatRoomName() == name) { broadcastRoomUsers(room); break; }
}

void Server::refreshRoomUsers(const std::string& name)
{
    auto room = std::find_if(roomsWithConnections.begin(), roomsWithConnections.end(),
        [&](const ChatRoom& r) { return r.getChatRoomName() == name; });
    if (room == roomsWithConnections.end()) return;
    std::vector<std::string> users;
    auto members = roomSockets.find(name);
    if (members != roomSockets.end())
        for (int fd : members->second) {
            auto auth = socketToUsername.find(fd);
            if (auth != socketToUsername.end() &&
                std::find(users.begin(), users.end(), auth->second) == users.end())
                users.push_back(auth->second);
        }
    std::sort(users.begin(), users.end());
    room->setConnectedUsers(std::move(users));
}

void Server::processProt5(int clientIndex, const std::string& plaintext)
{
    int sock = clientSocket[clientIndex];
    auto auth = socketToUsername.find(sock);
    if (auth == socketToUsername.end()) {
        sendProt5Error(sock, "-", "Log in before using chatrooms");
        return;
    }
    auto parts = splitByNewline(plaintext);
    if (parts.size() < 3 || !validProtocolName(parts[2], 32)) {
        sendProt5Error(sock, "-", "Malformed room request");
        return;
    }
    const std::string& command = parts[1];
    const std::string& name = parts[2];
    if (command == "CREATE" || command == "CONNECT") {
        if (parts.size() != 5 || parts[3] != auth->second ||
            !validBase64(parts[4], 44) || parts[4].size() != 44) {
            sendProt5Error(sock, name, "Malformed room credentials");
            return;
        }
        if (command == "CREATE") {
            if (!roomsDb.createRoom(name, auth->second, parts[4])) {
                sendProt5Error(sock, name, "Room already exists or database error");
                return;
            }
            // Creators automatically become connected members.
            joinRoom(sock, name, auth->second, parts[4]);
            for (const auto& [fd, user] : socketToUsername) sendFullRoomList(fd);
        } else joinRoom(sock, name, auth->second, parts[4]);
        return;
    }
    if (command == "LEAVE") {
        if (parts.size() != 3) { sendProt5Error(sock, name, "Malformed leave request"); return; }
        leaveRoom(sock, name);
        return;
    }
    if (command == "MESSAGE") {
        if (parts.size() != 4 || !validBase64(parts[3], 32 * 1024)) {
            sendProt5Error(sock, name, "Malformed encrypted message"); return;
        }
        auto it = roomSockets.find(name);
        if (it == roomSockets.end() || !it->second.count(sock)) {
            sendProt5Error(sock, name, "Join room before sending messages"); return;
        }
        if (!roomsDb.saveMessage(name, auth->second, parts[3])) {
            sendProt5Error(sock, name, "Could not save encrypted message"); return;
        }
        const std::string frame = "PROT5\nMESSAGE\n" + name + "\n" + parts[3];
        for (int fd : it->second) sendEncrypted(fd, frame);
        return;
    }
    sendProt5Error(sock, name, "Unknown room command");
}

// ROOM is metadata only. History arrives as binary-safe base64 MESSAGE packets.
void Server::sendRoom(int socket, const ChatRoom& room)
{
    std::string frame = "PROT5\nROOM\n" + room.getChatRoomName() +
        "\n-\nMESSAGES\nUSERS\n";
    for (const auto& user : room.getConnectedUsers()) frame += user + "\n";
    frame += "END\n" + room.getRoomCreator() + "\n" + room.getRoomCreationTime() + "\n";
    sendEncrypted(socket, frame);
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

        // Avoid holding the single-threaded event loop forever on an incomplete
        // handshake, partial frame, or peer that stops receiving.
        timeval networkTimeout{10, 0};
        setsockopt(newSocket, SOL_SOCKET, SO_RCVTIMEO, &networkTimeout, sizeof(networkTimeout));
        setsockopt(newSocket, SOL_SOCKET, SO_SNDTIMEO, &networkTimeout, sizeof(networkTimeout));

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

        handshakeUsernames[newSocket] = username;

        std::cout << "Handshake accepted: " << username << " from "
                << clientIp << ":" << clientPort << " (fd=" << newSocket << ")\n";

        // 5. Send OK reply (encrypted)
        std::string okPlain = "PROT2\nWelcome " + username + "!";
        std::string okCipher = FreiaEncryption::encryptData(okPlain, serverKey);
        if (okCipher.empty())
        {
            std::cerr << "[Critical] Failed to encrypt PROT2 reply\n";
            close(newSocket);
            handshakeUsernames.erase(newSocket);
            return;
        }

        if (!sendWithLengthPrefix(newSocket, okCipher))
        {
            std::cout << "Failed to send OK reply to " << username << "\n";
            close(newSocket);
            handshakeUsernames.erase(newSocket);
            return;
        }

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
            handshakeUsernames.erase(newSocket);
            return;
        }
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
        if (r != static_cast<int>(sizeof(packetLengthNet)))
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
            std::string errWarning = "[Warning] Invalid length: " + std::to_string(packetLength) + "\n";
            disconnectClient(i, errWarning);
            continue;
        }

        std::string encrypted(packetLength, '\0');
        r = recv(currentSocket, encrypted.data(), packetLength, MSG_WAITALL);
        if (r != static_cast<int>(packetLength))
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
        if (parts.empty()) { disconnectClient(i, "Empty protocol frame"); continue; }
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
            std::cerr << "[Protocol error] Unknown protocol from client\n";
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

// Names are persisted in SQLite and remain available after server restarts.
void Server::sendFullRoomList(int targetSocket)
{
    std::string list;
    for (const auto& name : roomsDb.roomNames()) {
        if (!list.empty()) list += "\n";
        list += name;
    }
    broadcastProt3(list, "roomList", targetSocket);
}

void Server::disconnectClient(int index, const std::string& reason)
{
    int fd = clientSocket[index];
    if (fd <= 0) return;
    std::string name;
    auto account = socketToUsername.find(fd);
    if (account != socketToUsername.end()) {
        name = account->second;
        socketToUsername.erase(account);
    }
    handshakeUsernames.erase(fd);
    std::vector<std::string> affected;
    for (auto it = roomSockets.begin(); it != roomSockets.end();) {
        if (it->second.erase(fd)) affected.push_back(it->first);
        if (it->second.empty()) it = roomSockets.erase(it);
        else ++it;
    }
    closeClientSocket(index);
    for (const auto& roomName : affected) {
        refreshRoomUsers(roomName);
        for (const auto& room : roomsWithConnections)
            if (room.getChatRoomName() == roomName) { broadcastRoomUsers(room); break; }
    }
    if (!name.empty()) {
        bool anotherSession = false;
        for (const auto& entry : socketToUsername)
            if (entry.second == name) { anotherSession = true; break; }
        if (!anotherSession) {
            broadcastProt3(name + " disconnected.", "userDisconnected");
            broadcastProt3(name, "userLeft");
        }
    }
    std::cerr << "Client disconnected (" << reason << "): fd=" << fd
              << " user=" << (name.empty() ? "unauthenticated" : name) << "\n";
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


// ========================= Other ======================

void Server::broadcastRoomUsers(const ChatRoom& room)
{
    const auto& roomName = room.getChatRoomName();
    auto members = roomSockets.find(roomName);
    if (members == roomSockets.end()) return;
    const auto users = room.getConnectedUsers();
    std::string frame = "PROT5\nUSERS\n" + roomName + "\n" + std::to_string(users.size());
    for (const auto& user : users) frame += "\n" + user;
    std::string encrypted = FreiaEncryption::encryptData(frame, serverKey);
    for (int fd : members->second)
        if (!encrypted.empty()) sendWithLengthPrefix(fd, encrypted);
}

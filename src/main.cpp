#include <iostream>
#include <string>
#include "server.h"

int main()
{
    std::cout << "Freia Thiwi v" << PROJECT_VERSION << "\n";

    int PORT;
    int maxClients;
    std::string serverPassword;

    std::cout << "Enter Port (1024–65535): ";
    std::cin >> PORT;
    if (PORT < 1024 || PORT > 65535) {
        std::cerr << "Invalid port\n";
        return 1;
    }

    std::cout << "Maximum number of clients: ";
    std::cin >> maxClients;
    if (maxClients <= 0) {
        std::cerr << "Invalid max clients\n";
        return 1;
    }

    std::cin.ignore();  // clear newline
    std::cout << "Server Password: ";
    std::getline(std::cin, serverPassword);
    if (serverPassword.empty()) {
        std::cerr << "Password cannot be empty\n";
        return 1;
    }

    Server server(PORT, maxClients, serverPassword);
    server.run();

    return 0;
}
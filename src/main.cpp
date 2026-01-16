#include <string>
#include <sstream>
#include "server.h"
#include "encrypt.h"


void packageTest();

int main()
{
    std::cout << "Freia Thiwi v" << PROJECT_VERSION << "\n";

    int maxClients;
    int PORT;

    std::cout << "Enter Port: ";
    std::cin >> PORT;
    std::cout << "Maximum number of clients: ";
    std::cin >> maxClients;

    Server server(PORT, maxClients);
    server.run();
    
    return 0;
}

void packageTest()
{
    // package test section
    std::string test = "for something epic!";
    std::string password = "test2";
    std::string password2 = "yippee!";

    std::string encryptedData = encryptData(test, password);

    std::string encryptedDataB64 = base64_encode(encryptedData);  // You must implement this

    std::string innerPackage = "PROT1\n" + encryptedDataB64;
    std::string fullPackage = encryptData(innerPackage, password2);

    std::string package = decryptData(fullPackage, password2);
    std::istringstream ss(package);
    std::string protocol;
    std::string encryptedMessageB64;

    std::getline(ss, protocol);           // "PROT1"
    std::getline(ss, encryptedMessageB64); // Full base64 string (no \n inside)

    std::string encryptedMessage = base64_decode(encryptedMessageB64);
    std::string plaintext = decryptData(encryptedMessage, password);

    std::cout << plaintext << "\n";

}
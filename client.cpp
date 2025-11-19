#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {

    // connect to Load Balancer
    int lbSock = socket(AF_INET, SOCK_STREAM, 0);
    if (lbSock < 0) { std::cerr << "Socket error\n"; return 1; }

    sockaddr_in lbAddr{};
    lbAddr.sin_family = AF_INET;
    lbAddr.sin_port = htons(9000);  // LB port
    inet_pton(AF_INET, "127.0.0.1", &lbAddr.sin_addr);

    if (connect(lbSock, (sockaddr*)&lbAddr, sizeof(lbAddr)) < 0) {
        std::cerr << "Failed to connect to Load Balancer\n";
        return 1;
    }

    // send client registration request
    send(lbSock, "CLIENT", 6, 0);

    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    recv(lbSock, buffer, sizeof(buffer), 0);

    std::string response(buffer);
    std::cout << "[CLIENT] LB Response: " << response << std::endl;

    if (response.find("NO_SERVERS_AVAILABLE") != std::string::npos) {
        std::cout << "No free servers right now. Try later.\n";
        close(lbSock);
        return 0;
    }

    // parse connect port
    int serverPort = 0;
    if (response.find("CONNECT") != std::string::npos) {
        serverPort = std::stoi(response.substr(8)); // extract port after connect
    }

    close(lbSock);  // disconnect from LB

    // connect to the assigned server
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) { std::cerr << "Socket error\n"; return 1; }

    sockaddr_in srvAddr{};
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, "127.0.0.1", &srvAddr.sin_addr);

    std::cout << "[CLIENT] Connecting to assigned server on port " << serverPort << "...\n";

    if (connect(serverSock, (sockaddr*)&srvAddr, sizeof(srvAddr)) < 0) {
        std::cerr << "Failed to connect to target server\n";
        return 1;
    }

    std::cout << "[CLIENT] Connected to server. Type messages to chat.\n";

    // chat loop
    std::string msg;
    char recvBuf[256];

    while (true) {
        std::cout << "You: ";
        std::getline(std::cin, msg);

        if (msg == "quit" || msg == "exit") break;

        send(serverSock, msg.c_str(), msg.size(), 0);

        memset(recvBuf, 0, sizeof(recvBuf));
        int n = recv(serverSock, recvBuf, sizeof(recvBuf), 0);
        if (n <= 0) {
            std::cout << "[CLIENT] Server disconnected.\n";
            break;
        }

        std::cout << "Server: " << recvBuf << std::endl;
    }

    close(serverSock);
    std::cout << "[CLIENT] Chat ended.\n";

    return 0;
}


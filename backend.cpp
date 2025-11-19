#include <iostream>
#include <thread>
#include <mutex>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

bool busy = false;
std::mutex busyMutex;  // for two threads which is the client chat thread and server health response thread

// thread to respond to LB health checks
void pingThread(int lbSock) {
    char buffer[16];

    while (true) {
        int n = recv(lbSock, buffer, sizeof(buffer), 0);
        if (n > 0) {
            std::string msg(buffer, n);
            if (msg.find("PING") != std::string::npos) {
                std::lock_guard<std::mutex> lock(busyMutex);
                if (busy) {
                    send(lbSock, "BUSY", 4, 0);
                    std::cout << "[SERVER] LB ping -> BUSY\n";
                } else {
                    send(lbSock, "FREE", 4, 0);
                    std::cout << "[SERVER] LB ping -> FREE\n";
                }
            }
        }
        // if n == 0 or -1, just continue waiting
    }
}

// thread to accept exactly one client and handle chat
void clientThread(int serverSock) {
    while (true) {
        sockaddr_in cliAddr{};
        socklen_t len = sizeof(cliAddr);

        std::cout << "[SERVER] Waiting for a client...\n";
        int clientSock = accept(serverSock, (sockaddr*)&cliAddr, &len);
        if (clientSock < 0) continue;

        {
            std::lock_guard<std::mutex> lock(busyMutex);
            busy = true;
        }

        std::cout << "[SERVER] Client connected.\n";

        char buffer[256];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int n = recv(clientSock, buffer, sizeof(buffer), 0);

            if (n <= 0) {
                std::cout << "[SERVER] Client disconnected.\n";
                close(clientSock);
                break;
            }

            std::cout << "[CLIENT]: " << buffer << std::endl;

            // echo back
            send(clientSock, buffer, strlen(buffer), 0);
        }

        {
            std::lock_guard<std::mutex> lock(busyMutex);
            busy = false;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./backend <server_port>\n";
        return 1;
    }

    int serverPort = atoi(argv[1]);

    // Connect to Load Balancer
    int lbSock = socket(AF_INET, SOCK_STREAM, 0);
    if (lbSock < 0) { perror("LB socket"); return 1; }

    sockaddr_in lbAddr{};
    lbAddr.sin_family = AF_INET;
    lbAddr.sin_port = htons(9000);           // LB port
    inet_pton(AF_INET, "127.0.0.1", &lbAddr.sin_addr);

    if (connect(lbSock, (sockaddr*)&lbAddr, sizeof(lbAddr)) < 0) {
        perror("Failed to connect to LB");
        return 1;
    }

    // send registration message including server's listening port
    std::string regMsg = "SERVER " + std::to_string(serverPort);
    send(lbSock, regMsg.c_str(), regMsg.size(), 0);
    std::cout << "[SERVER] Registered with Load Balancer." << std::endl;

    // create server socket for clients
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) { perror("server socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    // listen for clients
    if (listen(serverSock, 1) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "[SERVER] Running on port " << serverPort << "\n";

    // start threads
    std::thread t1(pingThread, lbSock);
    std::thread t2(clientThread, serverSock);

    t1.join();
    t2.join();

    close(serverSock);
    return 0;
}


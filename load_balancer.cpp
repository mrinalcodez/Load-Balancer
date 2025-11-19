#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>

struct ServerNode {
    int sock;           // socket descriptor for the server
    std::string ip;     // server IP
    int port;           // actual listening port for clients
    bool busy;          // server busy state
};

std::vector<ServerNode> servers;

// Remove server from list
void removeServer(int sock) {
    servers.erase(std::remove_if(servers.begin(), servers.end(),
        [&](const ServerNode &s){ return s.sock == sock; }), servers.end());
}


// Health check: ping all servers and update busy status
void healthCheck() {
    char ping[5] = "PING";
    char resp[16];

    for (auto it = servers.begin(); it != servers.end();) {
        // Send ping to check whether server is alive or not
        if (send(it->sock, ping, 4, 0) <= 0) {
            std::cout << "[LB] Server down, removing: " << it->ip << ":" << it->port << "\n";
            close(it->sock);
            it = servers.erase(it);
            continue;
        }

        // Wait for server response (blocking) for the server status
        memset(resp, 0, sizeof(resp));
        int n = recv(it->sock, resp, sizeof(resp), 0);
        if (n <= 0) {
            std::cout << "[LB] Server not responding, removing: " << it->ip << ":" << it->port << "\n";
            close(it->sock);
            it = servers.erase(it);
            continue;
        }

        std::string reply(resp, n);
        if (reply.find("BUSY") != std::string::npos) {
            it->busy = true;
            std::cout << "[LB] Server " << it->ip << ":" << it->port << " -> BUSY\n";
        } else {
            it->busy = false;
            std::cout << "[LB] Server " << it->ip << ":" << it->port << " -> FREE\n";
        }

        ++it;
    }
}


// Find first free server and return its listening port otherwise -1
int findFreeServer() {
    for (auto &s : servers) {
        if (!s.busy) return s.port;
    }
    return -1;
}

int main() {
    // Socket creation
    int lbSock = socket(AF_INET, SOCK_STREAM, 0);
    if (lbSock < 0) { std::cerr << "Socket creation failed\n"; return 1; }

    // Socket port setup
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);      // LB listens on 9000
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(lbSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    // listen for incoming connections
    if (listen(lbSock, 20) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "[LB] Load balancer running on port 9000\n";

    while (true) {
        // accept server and client connections
        sockaddr_in cliAddr{};
        socklen_t len = sizeof(cliAddr);
        int newSock = accept(lbSock, (sockaddr*)&cliAddr, &len);
        if (newSock < 0) continue;

        char buffer[128];
        memset(buffer, 0, sizeof(buffer));
        int n = recv(newSock, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) {
            close(newSock);
            continue;
        }

        std::string msg(buffer, n);
        // trim trailing whitespace/newline
        msg.erase(msg.find_last_not_of(" \n\r\t")+1);

        // Register Server
        if (msg.rfind("SERVER", 0) == 0) {  // message starts with "SERVER"
            int port = 0;
            if (msg.size() > 6) {
                // parse port if present
                try { port = std::stoi(msg.substr(7)); } 
                catch(...) { port = 0; }
            }

            if (port == 0) {
                std::cerr << "[LB] Warning: server registration missing port. Ignored.\n";
                close(newSock);  // close server socket
                continue;
            }

            ServerNode node{newSock, inet_ntoa(cliAddr.sin_addr), port, false};
            servers.push_back(node);
            std::cout << "[LB] Server registered: " << node.ip 
                      << ":" << node.port << " (sock=" << newSock << ")\n";
            continue;
        }

        // Handle Client Request
        if (msg.find("CLIENT") != std::string::npos) {
            std::cout << "[LB] Client request received. Performing health check...\n";

            healthCheck();
            int srvPort = findFreeServer();

            if (srvPort != -1) {
                std::string connectMsg = "CONNECT " + std::to_string(srvPort);
                send(newSock, connectMsg.c_str(), connectMsg.size(), 0);
                std::cout << "[LB] Client assigned to server on port " << srvPort << "\n";
            } else {
                std::string noMsg = "NO_SERVERS_AVAILABLE";
                send(newSock, noMsg.c_str(), noMsg.size(), 0);
                std::cout << "[LB] No free servers available\n";
            }

            close(newSock); // close client socket
        }
    }

    close(lbSock);
    return 0;
}


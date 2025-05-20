#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <map>
#include <iostream>
#include <thread>
#include <vector>
#include <list>
#include <algorithm>
#include <chrono>

using namespace std;

// Game constants and state (same as before)

class Server {
    map<string, struct sockaddr_in> clients;
    GameState game;
    
public:
    void handleClient(int socketFD, struct sockaddr_in clientAddr, const string& clientName) {
        clients[clientName] = clientAddr;
        
        while (true) {
            string message = receivePackets(socketFD, &clientAddr);
            if (message.empty()) break;
            
            const char* buffer = message.c_str();
            int messageLength = atoi(string(buffer, 5).c_str());
            char messageType = buffer[5];
            
            switch (messageType) {
                case 'L': handleListRequest(socketFD, clientAddr); break;
                case 'M': handlePrivateMessage(socketFD, clientAddr, clientName, messageLength, buffer + 6); break;
                case 'B': handleBroadcast(socketFD, clientAddr, clientName, messageLength, buffer + 6); break;
                case 'Q': handleQuit(clientName); return;
                case 'J': handleJoinGame(socketFD, clientAddr, clientName); break;
                case 'V': handleViewGame(socketFD, clientAddr, clientName); break;
                case 'P': handlePlayerMove(socketFD, clientAddr, clientName, buffer + 6); break;
                case 'F': handleFileTransfer(socketFD, clientAddr, clientName, buffer + 6); break;
                default: break;
            }
        }
        
        clients.erase(clientName);
    }
    
private:
    void sendPackets(int socketFD, struct sockaddr_in clientAddr, const char* data, int dataLength) {
        int totalPackets = (dataLength + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;
        
        for (int i = 0; i < totalPackets; i++) {
            char packet[BUFFER_SIZE];
            memset(packet, '#', BUFFER_SIZE);
            
            packet[0] = (i >> 8) & 0xFF;
            packet[1] = i & 0xFF;
            packet[2] = (totalPackets >> 8) & 0xFF;
            packet[3] = totalPackets & 0xFF;
            
            int offset = i * MAX_DATA_SIZE;
            int chunkSize = min(MAX_DATA_SIZE, dataLength - offset);
            memcpy(packet + 4, data + offset, chunkSize);
            
            sendto(socketFD, packet, BUFFER_SIZE, 0, 
                  (struct sockaddr*)&clientAddr, sizeof(clientAddr));
        }
    }
    
    string receivePackets(int socketFD, struct sockaddr_in* clientAddr) {
        char buffer[BUFFER_SIZE];
        socklen_t addrLen = sizeof(*clientAddr);
        
        recvfrom(socketFD, buffer, BUFFER_SIZE, 0, 
                (struct sockaddr*)clientAddr, &addrLen);
        
        int totalPackets = (buffer[2] << 8) | buffer[3];
        vector<string> packets(totalPackets);
        
        int packetNum = (buffer[0] << 8) | buffer[1];
        packets[packetNum] = string(buffer + 4, MAX_DATA_SIZE);
        
        for (int i = 1; i < totalPackets; i++) {
            recvfrom(socketFD, buffer, BUFFER_SIZE, 0, 
                    (struct sockaddr*)clientAddr, &addrLen);
            
            packetNum = (buffer[0] << 8) | buffer[1];
            packets[packetNum] = string(buffer + 4, MAX_DATA_SIZE);
        }
        
        string fullMessage;
        for (const auto& packet : packets) {
            fullMessage += packet;
        }
        
        size_t endPos = fullMessage.find_last_not_of('#');
        if (endPos != string::npos) {
            fullMessage = fullMessage.substr(0, endPos + 1);
        }
        
        return fullMessage;
    }
    
    // ... (other methods similarly modified to use UDP)
};

int main() {
    Server server;
    int socketFD = socket(PF_INET, SOCK_DGRAM, 0);
    
    if (socketFD == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1100);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketFD, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) {
        perror("error bind failed");
        close(socketFD);
        exit(EXIT_FAILURE);
    }

    while (true) {
        struct sockaddr_in clientAddr;
        string message = server.receivePackets(socketFD, &clientAddr);
        
        if (message.size() < 6) continue;
        
        int messageLength = atoi(string(message, 0, 5).c_str());
        char messageType = message[5];
        
        if (messageType == 'N') {
            string clientName = message.substr(6, messageLength - 1);
            printf("!!bienvenido!! --> %s\n", clientName.c_str());
            
            thread([&server, socketFD, clientAddr, clientName]() {
                server.handleClient(socketFD, clientAddr, clientName);
            }).detach();
        }
    }

    close(socketFD);
    return 0;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <openssl/sha.h>

using namespace std;

// Constants
const int BUFFER_SIZE = 500;
const int MAX_DATA_SIZE = BUFFER_SIZE - 4; // 4 bytes for header
const char* RECIPIENT_PROMPT = "para";
const char* MESSAGE_PROMPT = "msg";

// Global variables
bool controlmsg = true;
char currentPlayerSymbol;
vector<string> menuOptions = {
    "1. Ver lista de usuarios",
    "2. Enviar mensaje",
    "3. Enviar mensaje a todos",
    "4. Enviar archivo a usuario",
    "5. Unirse al juego",
    "6. Enviar posición",
    "7. Ver juego",
    "8. Salir"
};

// Function to split message into packets
void sendPackets(int socketFD, struct sockaddr_in serverAddr, const char* data, int dataLength) {
    int totalPackets = (dataLength + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;
    
    for (int i = 0; i < totalPackets; i++) {
        char packet[BUFFER_SIZE];
        memset(packet, '#', BUFFER_SIZE); // Fill with padding
        
        // Add header (2 bytes packet number, 2 bytes total packets)
        packet[0] = (i >> 8) & 0xFF;
        packet[1] = i & 0xFF;
        packet[2] = (totalPackets >> 8) & 0xFF;
        packet[3] = totalPackets & 0xFF;
        
        int offset = i * MAX_DATA_SIZE;
        int chunkSize = min(MAX_DATA_SIZE, dataLength - offset);
        memcpy(packet + 4, data + offset, chunkSize);
        
        sendto(socketFD, packet, BUFFER_SIZE, 0, 
              (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }
}

// Function to receive packets
string receivePackets(int socketFD, struct sockaddr_in* serverAddr) {
    char buffer[BUFFER_SIZE];
    socklen_t addrLen = sizeof(*serverAddr);
    
    // Receive first packet to get total packets
    recvfrom(socketFD, buffer, BUFFER_SIZE, 0, 
            (struct sockaddr*)serverAddr, &addrLen);
    
    int totalPackets = (buffer[2] << 8) | buffer[3];
    vector<string> packets(totalPackets);
    
    // Process first packet
    int packetNum = (buffer[0] << 8) | buffer[1];
    packets[packetNum] = string(buffer + 4, MAX_DATA_SIZE);
    
    // Receive remaining packets
    for (int i = 1; i < totalPackets; i++) {
        recvfrom(socketFD, buffer, BUFFER_SIZE, 0, 
                (struct sockaddr*)serverAddr, &addrLen);
        
        packetNum = (buffer[0] << 8) | buffer[1];
        packets[packetNum] = string(buffer + 4, MAX_DATA_SIZE);
    }
    
    // Combine packets
    string fullMessage;
    for (const auto& packet : packets) {
        fullMessage += packet;
    }
    
    // Remove padding
    size_t endPos = fullMessage.find_last_not_of('#');
    if (endPos != string::npos) {
        fullMessage = fullMessage.substr(0, endPos + 1);
    }
    
    return fullMessage;
}

// Menu functions (modified to use UDP)
void listUsers(int socketFD, struct sockaddr_in serverAddr) {
    char buffer[8] = "00001L\0";
    sendPackets(socketFD, serverAddr, buffer, 6);
}

void sendMessage(int socketFD, struct sockaddr_in serverAddr) {
    char recipient[101];
    char message[201];
    char finalBuffer[301];

    printf("%s : ", RECIPIENT_PROMPT);
    fflush(stdout);
    fgets(recipient, 100, stdin);
    int recipientLength = strlen(recipient);
    recipient[recipientLength - 1] = '\0'; 
    
    printf("%s : ", MESSAGE_PROMPT);
    fflush(stdout);
    fgets(message, 200, stdin);
    int messageLength = strlen(message);
    message[messageLength - 1] = '\0';

    sprintf(finalBuffer, "%05dM%05d%s%05d%s", 
            recipientLength + messageLength - 1, 
            messageLength - 1, message, 
            recipientLength - 1, recipient);
    
    sendPackets(socketFD, serverAddr, finalBuffer, recipientLength + messageLength + 14);
}

// ... (other menu functions similarly modified)

void* readSocketThread(void* arg) {
    int socketFD = *(int*)arg;
    struct sockaddr_in serverAddr;
    
    while (true) {
        string message = receivePackets(socketFD, &serverAddr);
        const char* buffer = message.c_str();
        
        int messageLength = atoi(string(buffer, 5).c_str());
        char messageType = buffer[5];
        
        switch (messageType) {
            case 'l': {
                printf("\nLista de usuarios:\n%s\n", buffer + 6);
                break;
            }
            case 'm': {
                int msgLength = atoi(string(buffer + 6, 5).c_str());
                string msgContent(buffer + 11, msgLength);
                int senderLength = atoi(string(buffer + 11 + msgLength, 5).c_str());
                string sender(buffer + 16 + msgLength, senderLength);
                
                printf("\nMensaje de <%s>: %s\n", sender.c_str(), msgContent.c_str());
                break;
            }
            // ... (other cases similarly modified)
        }
    }
    
    close(socketFD);
    return NULL;
}

int main() {
    struct sockaddr_in serverAddr;
    int socketFD = socket(PF_INET, SOCK_DGRAM, 0);

    if (socketFD == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1100);
    
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        perror("invalid address");
        close(socketFD);
        exit(EXIT_FAILURE);
    }

    char name[150];
    pthread_t thread;
    
    printf("nombre: ");
    fflush(stdout);
    fgets(name, 100, stdin);
    int nameLength = strlen(name);
    name[nameLength - 1] = '\0'; 
    
    char buffer[256];
    sprintf(buffer, "%05dN%s", nameLength + 1, name);
    sendPackets(socketFD, serverAddr, buffer, nameLength + 6);
    
    pthread_create(&thread, NULL, readSocketThread, &socketFD);
    pthread_detach(thread);

    while (true) {
        int option = 0;    
        displayMenu();
        cin >> option;
        cin.ignore();

        handleMenuOption(option, socketFD, serverAddr);
        
        if (option == 8) break;
    }

    close(socketFD);
    return 0;
}

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
const int BUFFER_SIZE = 300;
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

// Function declarations
void* readSocketThread(void* arg);
void listUsers(int socketFD);
void sendMessage(int socketFD);
void broadcastMessage(int socketFD);
void sendFile(int socketFD);
void joinGame(int socketFD);
void sendPosition(int socketFD);
void viewGame(int socketFD);
void quit(int socketFD);
void displayMenu();
void handleMenuOption(int option, int socketFD);

// Thread function to read from socket
void* readSocketThread(void* arg) {
    int clientSocket = *(int*)arg;
    char buffer[BUFFER_SIZE];
    int bytesRead;

    while (true) {
        bytesRead = read(clientSocket, buffer, 5);
        
        if (bytesRead <= 0) {
            printf("\n[Cliente] Conexión cerrada o error. Cerrando hilo...\n");
            break;
        }
        
        buffer[bytesRead] = '\0';
        int messageLength = atoi(buffer);
        
        bytesRead = read(clientSocket, buffer, 1);
        if (bytesRead <= 0) break;

        switch (buffer[0]) {
            case 'l': {
                buffer[0] = '\0';   
                bytesRead = read(clientSocket, buffer, messageLength - 1);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';
                printf("\nLista de usuarios:\n%s\n", buffer);
                break;
            }
            case 'm': {
                char senderBuffer[100];
                bytesRead = read(clientSocket, buffer, 5);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';
                int msgLength = atoi(buffer);

                bytesRead = read(clientSocket, buffer, msgLength);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';

                bytesRead = read(clientSocket, senderBuffer, 5);
                if (bytesRead <= 0) break;
                senderBuffer[bytesRead] = '\0';
                msgLength = atoi(senderBuffer);

                bytesRead = read(clientSocket, senderBuffer, msgLength);
                if (bytesRead <= 0) break;
                senderBuffer[bytesRead] = '\0';

                printf("\nMensaje de <%s>: %s\n", senderBuffer, buffer);
                break;
            }
            case 'b': {
                bytesRead = read(clientSocket, buffer, 5);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';
                int msgLength = atoi(buffer);

                bytesRead = read(clientSocket, buffer, msgLength);
                if (bytesRead <= 0) break;
                buffer[bytesRead] = '\0';

                char senderName[101];
                bytesRead = read(clientSocket, senderName, 5);
                if (bytesRead <= 0) break;
                senderName[bytesRead] = '\0';
                msgLength = atoi(senderName);

                bytesRead = read(clientSocket, senderName, msgLength);
                if (bytesRead <= 0) break;
                senderName[bytesRead] = '\0';

                printf("\nMensaje de <%s>: %s\n", senderName, buffer);
                break;
            }
            case 'f': {
                // File transfer handling (same as original)
                // ... [rest of file handling code]
                break;
            }
            case 'T': {
                bytesRead = read(clientSocket, buffer, 1);
                currentPlayerSymbol = buffer[0];
                printf("\nTu Turno %c \n", currentPlayerSymbol);
                break;
            }
            case 'E': {
                char errorBuffer[101];
                bytesRead = read(clientSocket, errorBuffer, 1);
                errorBuffer[bytesRead] = '\0';
                int errorNumber = atoi(errorBuffer);
                
                bytesRead = read(clientSocket, errorBuffer, 5);
                errorBuffer[bytesRead] = '\0';
                int errorLength = atoi(errorBuffer);
                
                bytesRead = read(clientSocket, errorBuffer, errorLength);
                errorBuffer[bytesRead] = '\0';

                printf("\nError <%01d> %s\n", errorNumber, errorBuffer);
                break;
            }
            case 'X': {
                char gameState[15];
                bytesRead = read(clientSocket, gameState, 9);
                gameState[bytesRead] = '\0';
                printf("\n");
                for (int i = 0; i < 9; i++) {
                    printf(" %c ", gameState[i]);
                    if (i % 3 != 2) printf("|");
                    if (i % 3 == 2 && i != 8) printf("\n---+---+---\n");
                }
                printf("\n");
                break;
            }
            case 'O': {
                char result[4];
                bytesRead = read(clientSocket, result, 1);
                result[1] = '\0';  
                if (bytesRead <= 0) {
                    perror("Error al leer el resultado o conexión cerrada");
                } else {
                    printf("\nRESULTADO DEL JUEGO : %c", result[0]);
                    fflush(stdout);
                }
                break;
            }
        }
    }

    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);
    return NULL;
}

// Menu functions
void listUsers(int socketFD) {
    char buffer[8] = "00001L\0";
    write(socketFD, buffer, 6);
}

void sendMessage(int socketFD) {
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
    
    write(socketFD, finalBuffer, recipientLength + messageLength + 14);
}

void broadcastMessage(int socketFD) {
    char message[101];
    char finalBuffer[301];

    printf("mensaje : ");
    fflush(stdout);
    fgets(message, 100, stdin);
    int messageLength = strlen(message);
    message[messageLength - 1] = '\0'; 
    
    sprintf(finalBuffer, "%05dB%05d%s", messageLength + 5, messageLength - 1, message);
    write(socketFD, finalBuffer, messageLength + 10);
}

void sendFile(int socketFD) {
    // File transfer implementation (same as original)
    // ... [rest of file sending code]
}

void joinGame(int socketFD) {
    char buffer[8] = "00001J\0";
    write(socketFD, buffer, 6);
    cout << "SOLICITANDO\n";
}

void sendPosition(int socketFD) {
    int position = 0;
    printf("\nposicion: ");
    cin >> position;
    cin.ignore(); 
    
    if (position < 0 || position > 9) {
        printf("\nposicion no valida\n");
        return;
    }
    
    char buffer[20];
    sprintf(buffer, "%05dP%01d%c", 3, position, currentPlayerSymbol);
    write(socketFD, buffer, 8);
}

void viewGame(int socketFD) {
    char buffer[8] = "00001V\0";
    write(socketFD, buffer, 6);
}

void quit(int socketFD) {
    char buffer[8] = "00001Q\0";
    write(socketFD, buffer, 6);
    cout << "Saliendo del chat...\n";
}

void displayMenu() {
    cout << "\n--- MENÚ ---\n";
    for (const auto& option : menuOptions) {
        cout << option << endl;
    }
    cout << "Seleccione una opción: ";
}

void handleMenuOption(int option, int socketFD) {
    switch (option) {
        case 1: listUsers(socketFD); break;
        case 2: sendMessage(socketFD); break;
        case 3: broadcastMessage(socketFD); break;
        case 4: sendFile(socketFD); break;
        case 5: joinGame(socketFD); break;
        case 6: sendPosition(socketFD); break;
        case 7: viewGame(socketFD); break;
        case 8: quit(socketFD); break;
        default: cout << "Opción inválida. Intenta de nuevo.\n";
    }
}

int main() {
    struct sockaddr_in serverAddr;
    int socketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    char buffer[250];

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

    if (connect(socketFD, (const struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in)) {
        perror("connect failed");
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
    
    sprintf(buffer, "%05dN%s", nameLength + 1, name);
    write(socketFD, buffer, nameLength + 6);
    
    pthread_create(&thread, NULL, readSocketThread, &socketFD);
    pthread_detach(thread);

    while (true) {
        int option = 0;    
        displayMenu();
        cin >> option;
        cin.ignore();

        handleMenuOption(option, socketFD);
        
        if (option == 8) break;
    }

    shutdown(socketFD, SHUT_RDWR);
    close(socketFD);
    return 0;
}

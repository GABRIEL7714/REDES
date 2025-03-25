#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 1100
#define BUFFER_SIZE 256

int socketFD;

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int n;

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        n = read(socketFD, buffer, BUFFER_SIZE - 1);
        if (n <= 0) {
            printf("Desconectado del servidor.\n");
            break;
        }
        printf("Servidor: %s\n", buffer);
    }

    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    pthread_t thread;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(socketFD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en connect");
        close(socketFD);
        exit(EXIT_FAILURE);
    }

    printf("Conectado al servidor.\n");

    // Crear un hilo para recibir mensajes del servidor
    pthread_create(&thread, NULL, receive_messages, NULL);

    while (1) {
        printf("Tú: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        write(socketFD, buffer, strlen(buffer));
    }

    close(socketFD);
    return 0;
}

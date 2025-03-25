#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 1100
#define BUFFER_SIZE 256

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    int n;

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        n = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (n <= 0) {
            printf("Cliente desconectado.\n");
            break;
        }
        printf("Cliente: %s\n", buffer);

        // Enviar respuesta
        printf("Servidor: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        write(client_socket, buffer, strlen(buffer));
    }

    close(client_socket);
    return NULL;
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    int server_socket, client_socket;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    listen(server_socket, 5);
    printf("Servidor escuchando en el puerto %d...\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error en accept");
            continue;
        }
        printf("Cliente conectado.\n");

        // Crear un hilo para manejar al cliente
        pthread_create(&thread, NULL, handle_client, (void *)&client_socket);
    }

    close(server_socket);
    return 0;
}

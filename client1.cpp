#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

void recibir_mensajes(int socket_fd) {
    char buffer[1024];
    while (true) {
        int n = read(socket_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            cout << "Desconectado del servidor." << endl;
            break;
        }
        buffer[n] = '\0';
        cout << "\nMensaje recibido: " << buffer << endl;
    }
}

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Error al crear socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(46000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en connect");
        return 1;
    }

    cout << "Conectado al servidor. Escribe un mensaje:" << endl;

    thread t(recibir_mensajes, socket_fd);
    t.detach();

    char buffer[1024];
    while (true) {
        cout << "Mensaje: ";
        fgets(buffer, sizeof(buffer), stdin);
        write(socket_fd, buffer, strlen(buffer));

        if (strncmp(buffer, "chau", 4) == 0) {
            break;
        }
    }

    close(socket_fd);
    return 0;
}

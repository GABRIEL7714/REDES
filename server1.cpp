#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>

using namespace std;

vector<int> clientes;
mutex mtx;

void manejar_cliente(int client_fd) {
    char buffer[1024];
    while (true) {
        int n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            cout << "Cliente desconectado" << endl;
            break;
        }

        buffer[n] = '\0';
        cout << "Mensaje recibido: " << buffer;

        // Enviar el mensaje a todos los clientes
        mtx.lock();
        for (int cliente : clientes) {
            if (cliente != client_fd) {
                write(cliente, buffer, n);
            }
        }
        mtx.unlock();

        if (strncmp(buffer, "chau", 4) == 0) {
            break;
        }
    }

    close(client_fd);
    mtx.lock();
    clientes.erase(remove(clientes.begin(), clientes.end(), client_fd), clientes.end());
    mtx.unlock();
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Error al crear socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(46000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en bind");
        return 1;
    }

    if (listen(server_fd, 10) == -1) {
        perror("Error en listen");
        return 1;
    }

    cout << "Servidor escuchando en el puerto 46000..." << endl;

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == -1) {
            perror("Error en accept");
            continue;
        }

        cout << "Nuevo cliente conectado" << endl;
        mtx.lock();
        clientes.push_back(client_fd);
        mtx.unlock();

        thread t(manejar_cliente, client_fd);
        t.detach();
    }

    close(server_fd);
    return 0;
}

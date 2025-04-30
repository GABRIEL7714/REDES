#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>


using namespace std;
int resultado = 0;
string formatear_numero(int numero) {
    stringstream ss;
    ss << setw(10) << setfill('0') << numero;
    return ss.str();
}
void recibir_mensajes(int socket_cliente) {
    char buffer[100];
    while (true) {
        int bytes_recibidos = recv(socket_cliente, buffer, 1, 0);  // Primero recibes los 5 bytes de tamaño
        if (bytes_recibidos <= 0) {
            break;
        }

        buffer[bytes_recibidos] = '\0';
        if(buffer[0]=='M')
        {
            bytes_recibidos = recv(socket_cliente, buffer, 6, 0);
            int size_msg = atoi(buffer);
            bytes_recibidos = recv(socket_cliente, buffer, size_msg,0);
            if (bytes_recibidos <= 0) {
                break;
            }
            string msg1 = buffer;

            bytes_recibidos = recv(socket_cliente, buffer, 3,0);
            int size_msg2 = atoi(buffer);
            bytes_recibidos = recv(socket_cliente, buffer, size_msg2,0);
            if (bytes_recibidos <= 0) {
                break;
            }
            string msg2 = buffer;


            bytes_recibidos = recv(socket_cliente, buffer, 7, 0);
            int size_msg3 = atoi(buffer);
            bytes_recibidos = recv(socket_cliente, buffer, size_msg3,0);
            if (bytes_recibidos <= 0) {
                break;
            }
            string msg3 = buffer;


            bytes_recibidos = recv(socket_cliente, buffer, 1, 0);
            int size_msg4 = atoi(buffer);
            bytes_recibidos = recv(socket_cliente, buffer, size_msg4,0);
            if (bytes_recibidos <= 0) {
                break;
            }
            string msg4 = buffer;
            resultado++;
            break;

        }
        else if(buffer[0]=='F'){
            close(socket_cliente);
        }
                
    }
}
int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Error al crear socket");
        return 1;
    }else{
        cout<<"Socket creado correctamente";
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(48080);
    inet_pton(AF_INET, "172.16.16.149", &server_addr.sin_addr);

    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en connect");
        return 1;
    }
    else{
        cout<<"Conectado";
    }

    thread t(recibir_mensajes, socket_fd);
    t.detach();

    string tam = to_string(resultado);
    string tam2 = formatear_numero(static_cast<int>(tam.size()));

    string protocolo = 'R' + tam2 + tam + formatear_numero(8) + "VALDIVIA";
    write(socket_fd,protocolo.c_str(),static_cast<int>(protocolo.length()));

    close(socket_fd);
    return 0;
}

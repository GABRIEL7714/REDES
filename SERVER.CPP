#include <iostream>
#include <thread>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>

using namespace std;

std::map<string, int> mapSockets;
std::mutex mapMutex;

string formatear_numeron(int numero) {
    stringstream ss;
    ss << setw(2) << setfill('0') << numero;
    return ss.str();
}
string formatear_numerov(int numero) {
  stringstream ss;
  ss << setw(3) << setfill('0') << numero;
  return ss.str();
}

void readSocketThread(int cliSocket, char tag) {
    char buffer[300];
    int n;

    string miNombre = "";

    do {
       
        if (tag == 'l') {
            // Listar todos los usuarios
            string msg;
            {
                lock_guard<mutex> lock(mapMutex);
                msg = "L" + to_string(mapSockets.size());
                for (auto &it : mapSockets) {
                    msg += formatear_numeron((int)it.first.size()) + it.first;
                }
            }
            write(cliSocket, msg.c_str(), (int)msg.size());
        }

        else if (tag == 't') {
            char contenido[300];
            memset(contenido, 0, sizeof(contenido));

            n = read(cliSocket, contenido, 2);
            if (n <= 0) break;
            int tamano_destinatario = atoi(contenido);

            n = read(cliSocket, contenido, tamano_destinatario);
            if (n <= 0) break;
            contenido[n] = '\0';
            string destinatario = contenido;

            n = read(cliSocket, contenido, 3);
            if (n <= 0) break;
            int tamano_mensaje = atoi(contenido);

            n = read(cliSocket, contenido, tamano_mensaje);
            if (n <= 0) break;
            contenido[n] = '\0';
            string mensaje = contenido;

            if (mapSockets.find(destinatario) != mapSockets.end()) {
                string protocolo = "T" + formatear_numeron((int)miNombre.size()) + miNombre +
                                   formatear_numerov((int)mensaje.size()) + mensaje;
                write(mapSockets[destinatario], protocolo.c_str(), (int)protocolo.size());
            } else {
                string error = "Usuario no encontrado.";
                write(cliSocket, error.c_str(), (int)error.size());
            }
        }

        else if (tag == 'n') {
            n = read(cliSocket, buffer, 2);
            if (n <= 0) break;
            int tamano = atoi(buffer);

            n = read(cliSocket, buffer, tamano);
            if (n <= 0) break;
            buffer[n] = '\0';
            string newUser(buffer);

    
            mapSockets[newUser] = cliSocket;
            

            miNombre = newUser;
            cout << "Usuario " << miNombre << " añadido." << endl;
        }

        else if (buffer[0] == 'm') {
            char contenido[300];
            memset(contenido, 0, sizeof(contenido));

            n = read(cliSocket, contenido, 3);
            if (n <= 0) break;
            int tamano_mensaje = atoi(contenido);

            n = read(cliSocket, contenido, tamano_mensaje);
            if (n <= 0) break;
            contenido[n] = '\0';
            string mensaje = contenido;

            string enviador = "";
            {
                lock_guard<mutex> lock(mapMutex);
                for (auto &it : mapSockets) {
                    if (it.second == cliSocket) {
                        enviador = it.first;
                        break;
                    }
                }
            }

            string mensaje_ = "M" + formatear_numeron((int)enviador.size()) + enviador +
                              formatear_numeron((int)mensaje.size()) + mensaje;

            lock_guard<mutex> lock(mapMutex);
            for (const auto &[nombre, sock] : mapSockets) {
                if (sock != cliSocket) {
                    write(sock, mensaje_.c_str(), (int)mensaje_.size());
                }
            }
        }

        else if (buffer[0] == 'Q') {
            cout << "Cliente pidió salir." << endl;
            break;
        }

    } while (true);

    shutdown(cliSocket, SHUT_RDWR);
    close(cliSocket);

    if (!miNombre.empty()) {
        lock_guard<mutex> lock(mapMutex);
        mapSockets.erase(miNombre);
        cout << "Usuario " << miNombre << " desconectado." << endl;
    }
}

int main(void) {
    struct sockaddr_in stSockAddr;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    char buffer[256];

    if (SocketFD == -1) {
        perror("can not create socket");
        exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(45000);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)) == -1) {
        perror("error bind failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (listen(SocketFD, 10) == -1) {
        perror("error listen failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    cout << "Servidor iniciado en puerto 45000..." << endl;

    for (;;) {
        int ClientFD = accept(SocketFD, NULL, NULL);
        if (ClientFD < 0) {
            perror("error accept failed");
            close(SocketFD);
            exit(EXIT_FAILURE);
        }

        int n = read(ClientFD, buffer, 1);
        if (n <= 0) continue;

        std::thread(readSocketThread, ClientFD, buffer[0]).detach();
    }

    close(SocketFD);
    return 0;
}

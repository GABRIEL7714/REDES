#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <sys/stat.h>   // mkdir

using namespace std;

map<int, string> mapSockets;
mutex mapMutex;

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

string formatear_numerod(int numero) {
    stringstream ss;
    ss << setw(10) << setfill('0') << numero;
    return ss.str();
}

void broadcast(const string &msg, int remitente) {
    lock_guard<mutex> lock(mapMutex);
    for (auto &kv : mapSockets) {
        if (kv.first != remitente) {
            write(kv.first, msg.c_str(), msg.size());
        }
    }
}

void readSocketThread(int clientSock) {
    char buffer[1024];

    while (true) {
        int n = read(clientSock, buffer, 1);
        if (n <= 0) {
            lock_guard<mutex> lock(mapMutex);
            cout << "Cliente desconectado: " << mapSockets[clientSock] << endl;
            mapSockets.erase(clientSock);
            close(clientSock);
            return;
        }

        char tipo = buffer[0];

        if (tipo == 'n') {
            n = read(clientSock, buffer, 2);
            buffer[n] = '\0';
            int tam = atoi(buffer);
            n = read(clientSock, buffer, tam);
            string nombre(buffer, n);

            {
                lock_guard<mutex> lock(mapMutex);
                mapSockets[clientSock] = nombre;
            }
            cout << "Nuevo usuario registrado: " << nombre << endl;
        }
        else if (tipo == 't') {
            n = read(clientSock, buffer, 2);
            buffer[n] = '\0';
            int tamReceptor = atoi(buffer);
            n = read(clientSock, buffer, tamReceptor);
            string receptor(buffer, n);

            n = read(clientSock, buffer, 3);
            buffer[n] = '\0';
            int tamMsg = atoi(buffer);
            n = read(clientSock, buffer, tamMsg);
            string mensaje(buffer, n);

            string remitente;
            {
                lock_guard<mutex> lock(mapMutex);
                remitente = mapSockets[clientSock];
            }

            cout << remitente << " -> " << receptor << ": " << mensaje << endl;

            lock_guard<mutex> lock(mapMutex);
            for (auto &kv : mapSockets) {
                if (kv.second == receptor) {
                    string protocolo = "T" + formatear_numeron(remitente.size()) + remitente +
                                       formatear_numerov(mensaje.size()) + mensaje;
                    write(kv.first, protocolo.c_str(), protocolo.size());
                }
            }
        }
        else if (tipo == 'm') {
            n = read(clientSock, buffer, 3);
            buffer[n] = '\0';
            int tamMsg = atoi(buffer);
            n = read(clientSock, buffer, tamMsg);
            string mensaje(buffer, n);

            string remitente;
            {
                lock_guard<mutex> lock(mapMutex);
                remitente = mapSockets[clientSock];
            }

            cout << remitente << " (a todos): " << mensaje << endl;

            string protocolo = "M" + formatear_numeron(remitente.size()) + remitente +
                               formatear_numerov(mensaje.size()) + mensaje;
            broadcast(protocolo, clientSock);
        }
        else if (tipo == 'l') {
            lock_guard<mutex> lock(mapMutex);
            string lista = "L" + formatear_numeron(mapSockets.size());
            for (auto &kv : mapSockets) {
                lista += formatear_numeron(kv.second.size()) + kv.second;
            }
            write(clientSock, lista.c_str(), lista.size());
        }
        else if (tipo == 'f') {
            n = read(clientSock, buffer, 2);
            buffer[n] = '\0';
            int tamDest = atoi(buffer);
            n = read(clientSock, buffer, tamDest);
            buffer[n] = '\0';
            string destinatario(buffer,n);

            n = read(clientSock, buffer, 3);
            buffer[n] = '\0';
            int tamNom = atoi(buffer);
            n = read(clientSock, buffer, tamNom);
            string nombreArchivo(buffer, n);

            //modificamos nombre
            nombreArchivo.erase(nombreArchivo.length()-4,4);
            nombreArchivo+= "_dest.txt";
            n = read(clientSock, buffer, 10);
            buffer[n] = '\0';
            int tamArchivo = atoi(buffer);

            string datos;
            datos.resize(tamArchivo);
            int recibidos = 0;
            while (recibidos < tamArchivo) {
                int leidos = read(clientSock, &datos[recibidos], tamArchivo - recibidos);
                if (leidos <= 0) break;
                recibidos += leidos;
            }

            mkdir("archivos_recibidos", 0777);

            string ruta = "archivos_recibidos/" + nombreArchivo;
            ofstream out(ruta);
            out << datos;
            out.close();

            string remitente;
            {
                lock_guard<mutex> lock(mapMutex);
                remitente = mapSockets[clientSock];
            }

            cout << "Archivo recibido de " << remitente << ": " << ruta << endl;

            string protocolo = "F" + formatear_numerod((int)nombreArchivo.size()) + nombreArchivo + formatear_numeron(nombreArchivo.size()) + nombreArchivo +
                               formatear_numerov(datos.size()) + datos;
            broadcast(protocolo, clientSock);
        }
        else {
            cout << "Tipo desconocido recibido" << endl;
        }
    }
}

int main() {
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SocketFD == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in stSockAddr{};
    memset(&stSockAddr, 0, sizeof(stSockAddr));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(45000);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(SocketFD, (sockaddr *)&stSockAddr, sizeof(stSockAddr)) == -1) {
        perror("bind");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (listen(SocketFD, 10) == -1) {
        perror("listen");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    cout << "Servidor escuchando en puerto 45000..." << endl;

    while (true) {
        int clientSock = accept(SocketFD, NULL, NULL);
        if (clientSock < 0) {
            perror("accept");
            continue;
        }
        thread(readSocketThread, clientSock).detach();
    }

    close(SocketFD);
    return 0;
}

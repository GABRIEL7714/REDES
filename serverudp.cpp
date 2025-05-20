/* Server code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mutex>
#include <iostream>
#include <thread>
#include <map>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>

using namespace std;

// Global variables
std::unordered_map<std::string, sockaddr_in> mapaAddr;
std::unordered_map<std::string, std::vector<std::string>> clientePkgs;

struct Partida {
    char tablero[9] = {'_', '_', '_', '_', '_', '_', '_', '_', '_'};
    string jugadorO;
    string jugadorX;
    vector<string> espectadores;
    char turno = 'O';
    bool activa = false;
} partida;

string jugadorEnEspera;

// Function prototypes
void enviarM(int sock, const sockaddr_in &dest, const std::string &msg);
void enviarX_aTodos(int sock);
void enviarT(int sock, const std::string &nick, char simbolo);
void finalizarPartida(char resultado);
bool ganador(char s);
bool tableroLleno();
void procesarMensajes(std::vector<std::string> pkgs, std::string nickname, int sock, char tipo);
std::vector<std::string> partir(std::string msg, int tamMax);

// Split message into chunks
std::vector<std::string> partir(std::string msg, int tamMax) {
    std::vector<std::string> partes;
    int len = msg.size();

    for (int i = 0; i < len; i += tamMax) {
        partes.push_back(msg.substr(i, tamMax));
    }

    return partes;
}

// Send message to client
void enviarM(int sock, const sockaddr_in &dest, const std::string &msg) {
    const std::string remitente = "servidor";
    const int lenMsg = (int)msg.size();
    const int lenRem = (int)remitente.size();

    int tamTot = 1 + 5 + lenMsg + 5 + lenRem;
    char buffer[500];
    std::memset(buffer, '#', 500);

    std::memcpy(buffer, "0001", 4);
    std::sprintf(buffer + 4, "%05d", tamTot);
    int off = 9;
    buffer[off++] = 'm';
    std::sprintf(buffer + off, "%05d", lenMsg);
    off += 5;
    std::memcpy(buffer + off, msg.c_str(), lenMsg);
    off += lenMsg;
    std::sprintf(buffer + off, "%05d", lenRem);
    off += 5;
    std::memcpy(buffer + off, remitente.c_str(), lenRem);

    if (sendto(sock, buffer, 500, 0, (const sockaddr *)&dest, sizeof(dest)) == -1)
        perror("sendto");
}

// Send board to all players
void enviarX_aTodos(int sock) {
    char pkt[500];
    std::memset(pkt, '#', 500);
    std::memcpy(pkt, "0001", 4);
    std::memcpy(pkt + 4, "00010", 5);
    pkt[9] = 'X';
    std::memcpy(pkt + 10, partida.tablero, 9);

    for (auto nick : {partida.jugadorO, partida.jugadorX}) {
        if (!nick.empty())
            sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[nick], sizeof(sockaddr_in));
    }

    for (auto &esp : partida.espectadores) {
        sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[esp], sizeof(sockaddr_in));
    }
}

// Send turn notification
void enviarT(int sock, const std::string &nick, char simbolo) {
    char pkt[500];
    std::memset(pkt, '#', 500);
    std::memcpy(pkt, "0001", 4);
    std::memcpy(pkt + 4, "00002", 5);
    pkt[9] = 'T';
    pkt[10] = simbolo;

    sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[nick], sizeof(sockaddr_in));
}

// Check for winner
bool linea(int a, int b, int c, char s) {
    return partida.tablero[a] == s && partida.tablero[b] == s && partida.tablero[c] == s;
}

bool ganador(char s) {
    return linea(0, 1, 2, s) || linea(3, 4, 5, s) || linea(6, 7, 8, s) ||
           linea(0, 3, 6, s) || linea(1, 4, 7, s) || linea(2, 5, 8, s) ||
           linea(0, 4, 8, s) || linea(2, 4, 6, s);
}

bool tableroLleno() {
    for (char c : partida.tablero)
        if (c == '_')
            return false;
    return true;
}

// Process received messages
void procesarMensajes(std::vector<std::string> pkgs, std::string nickname, int sock, char tipo) {
    // MENSAJE L
    if (tipo == 'L') {
        int sizeFijo = 6;
        int tamMax = 500 - sizeFijo;

        std::string mensaje;
        std::string coma = "";
        for (auto it = mapaAddr.cbegin(); it != mapaAddr.cend(); ++it) {
            if ((*it).first != nickname) {
                mensaje += coma + (*it).first;
                coma = ",";
            }
        }
        sockaddr_in dest = mapaAddr[nickname];

        if (mensaje.length() > tamMax) {
            std::vector<std::string> piezas = partir(mensaje, tamMax);
            int seq = 1;
            int totalPkg = piezas.size();
            for (int i = 0; i < totalPkg; i++) {
                char buffer[500];
                std::memset(buffer, '#', 500);
                sprintf(buffer, "%02d%02d%05dl%s\0", seq++, totalPkg, (int)piezas[i].length() + 1, piezas[i].c_str());
                sendto(sock, buffer, 500, 0, (sockaddr *)&dest, sizeof(dest));
            }
        }
        else {
            char buffer[500];
            std::memset(buffer, '#', 500);
            sprintf(buffer, "0001%05dl%s\0", (int)mensaje.length() + 1, mensaje.c_str());
            sendto(sock, buffer, 500, 0, (sockaddr *)&dest, sizeof(dest));
        }
    }
    // MENSAJE M
    else if (tipo == 'M') {
        int sizeFijo = 20 + nickname.length();
        int tamMax = 500 - sizeFijo;

        int tamMsg1 = std::stoi(pkgs[0].substr(10, 5));
        int tamDest = std::stoi(pkgs[0].substr(15 + tamMsg1, 5));
        std::string destino = pkgs[0].substr(15 + tamMsg1 + 5, tamDest);

        std::string mensaje;
        for (size_t i = 0; i < pkgs.size(); ++i) {
            int t = std::stoi(pkgs[i].substr(10, 5));
            mensaje += pkgs[i].substr(15, t);
        }

        sockaddr_in dest = mapaAddr[destino];
        std::vector<std::string> piezas = (mensaje.length() > tamMax)
            ? partir(mensaje, tamMax)
            : std::vector<std::string>{mensaje};

        int totalPkg = piezas.size();
        int seq = 1;

        for (auto &trozo : piezas) {
            char buffer[500];
            std::memset(buffer, '#', 500);
            std::sprintf(buffer, "%02d%02d", (seq == totalPkg ? 0 : seq), totalPkg);
            int totalSize = 1 + 5 + trozo.length() + 5 + nickname.length();

            int offset = 4;
            std::sprintf(buffer + offset, "%05d", totalSize);
            offset += 5;
            buffer[offset++] = 'm';
            std::sprintf(buffer + offset, "%05d", (int)trozo.length());
            offset += 5;
            std::memcpy(buffer + offset, trozo.c_str(), trozo.length());
            offset += trozo.length();
            std::sprintf(buffer + offset, "%05d", (int)nickname.length());
            offset += 5;
            std::memcpy(buffer + offset, nickname.c_str(), nickname.length());

            sendto(sock, buffer, 500, 0, (sockaddr *)&dest, sizeof(dest));
            ++seq;
        }
    }
    // MENSAJE B
    else if (tipo == 'B') {
        int sizeFijo = 20 + nickname.length();
        int tamMax = 500 - sizeFijo;

        std::string mensaje;
        for (size_t i = 0; i < pkgs.size(); ++i) {
            int t = std::stoi(pkgs[i].substr(10, 5));
            mensaje += pkgs[i].substr(15, t);
        }

        std::vector<std::string> piezas = (mensaje.length() > tamMax)
            ? partir(mensaje, tamMax)
            : std::vector<std::string>{mensaje};

        int totalPkg = piezas.size();
        int seq = 1;

        for (auto &trozo : piezas) {
            char buffer[500];
            std::memset(buffer, '#', 500);
            std::sprintf(buffer, "%02d%02d", (seq == totalPkg ? 0 : seq), totalPkg);
            int totalSize = 1 + 5 + trozo.length() + 5 + nickname.length();

            int offset = 4;
            std::sprintf(buffer + offset, "%05d", totalSize);
            offset += 5;
            buffer[offset++] = 'b';
            std::sprintf(buffer + offset, "%05d", (int)trozo.length());
            offset += 5;
            std::memcpy(buffer + offset, trozo.c_str(), trozo.length());
            offset += trozo.length();
            std::sprintf(buffer + offset, "%05d", (int)nickname.length());
            offset += 5;
            std::memcpy(buffer + offset, nickname.c_str(), nickname.length());

            for (const auto &par : mapaAddr) {
                if (par.first == nickname) continue;
                const sockaddr_in &dest = par.second;
                sendto(sock, buffer, 500, 0, (const sockaddr *)&dest, sizeof(dest));
            }
            ++seq;
        }
    }
    // MENSAJE F
    else if (tipo == 'F') {
        int tamDest = std::stoi(pkgs[0].substr(10, 5));
        std::string destino = pkgs[0].substr(15, tamDest);
        int tamFileName = std::stoi(pkgs[0].substr(15 + tamDest, 100));
        std::string fileName = pkgs[0].substr(15 + tamDest + 100, tamFileName);
        long tamFile = std::stol(pkgs[0].substr(15 + tamDest + 100 + tamFileName, 18));
        std::string hash = pkgs[0].substr(15 + tamDest + 100 + tamFileName + 18 + tamFile, 5);

        int sizeFijo = 138 + nickname.length() + fileName.length();
        int tamMax = 500 - sizeFijo;

        std::string archivo;
        for (size_t i = 0; i < pkgs.size(); ++i) {
            long lenTrozo = std::stol(pkgs[i].substr(15 + tamDest + 100 + tamFileName, 18));
            size_t posDatos = 15 + tamDest + 100 + tamFileName + 18;
            archivo += pkgs[i].substr(posDatos, lenTrozo);
        }

        std::vector<std::string> piezas = (archivo.length() > tamMax)
            ? partir(archivo, tamMax)
            : std::vector<std::string>{archivo};

        int totalPkg = piezas.size();
        int seq = 1;

        for (auto &trozo : piezas) {
            char buffer[500];
            std::memset(buffer, '#', 500);
           

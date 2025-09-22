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

// ======================= CLASES ===========================
class Sillon {
    public:
        int plazas;
        string color;
        Sillon(int p=0, string c="") : plazas(p), color(c) {}
    };
    
    class Mesa {
    public:
        int patas;
        string material;
        Mesa(int pa=0, string m="") : patas(pa), material(m) {}
    };
    
    class Cocina {
    public:
        bool tieneHorno;
        string tipo;
        Cocina(bool h=false, string t="") : tieneHorno(h), tipo(t) {}
    };
    
    class Sala {
    public:
        Sillon sillon;
        Mesa mesa;
        Cocina cocina;
        int metros;
        char descripcion[1000];
    
        Sala() : metros(0) { descripcion[0] = '\0'; }
    
        // Serializar Sala en string para enviar por socket
        string serialize() const {
            stringstream ss;
            ss << sillon.plazas << "|" << sillon.color << "|"
               << mesa.patas << "|" << mesa.material << "|"
               << cocina.tieneHorno << "|" << cocina.tipo << "|"
               << metros << "|" << descripcion;
            return ss.str();
        }
    
        // Reconstruir Sala desde string recibido
        static Sala deserialize(const string &data) {
            Sala sala;
            stringstream ss(data);
            string token;
    
            getline(ss, token, '|'); sala.sillon.plazas = stoi(token);
            getline(ss, sala.sillon.color, '|');
            getline(ss, token, '|'); sala.mesa.patas = stoi(token);
            getline(ss, sala.mesa.material, '|');
            getline(ss, token, '|'); sala.cocina.tieneHorno = stoi(token);
            getline(ss, sala.cocina.tipo, '|');
            getline(ss, token, '|'); sala.metros = stoi(token);
            ss.getline(sala.descripcion, 1000);
    
            return sala;
        }
    };
    

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

ssize_t readN(int sock, void* buffer, size_t n) {
    size_t total = 0;
    char* buf = (char*)buffer;
    while (total < n) {
        ssize_t bytes = read(sock, buf + total, n - total);
        if (bytes <= 0) return -1; // error o desconexión
        total += bytes;
    }
    return total;
}

ssize_t writeN(int sock, const void* buffer, size_t n) {
    size_t total = 0;
    const char* buf = (const char*)buffer;
    while (total < n) {
        ssize_t bytes = write(sock, buf + total, n - total);
        if (bytes <= 0) return -1;
        total += bytes;
    }
    return total;
}

void readSocketThread(int cliSocket) {
    char buffer[300];
    int n;
    string miNombre = "";

    while (true) {
        // Leer comando (1 byte)
        n = readN(cliSocket, buffer, 1);
        if (n <= 0) break;

        char cmd = buffer[0];

        if (cmd == 'l') {
            // Listar usuarios
            string msg;
            {
                lock_guard<mutex> lock(mapMutex);
                msg = "L" + to_string(mapSockets.size());
                for (auto &it : mapSockets) {
                    msg += formatear_numeron((int)it.first.size()) + it.first;
                }
            }
            cout<<"Protocolo : "<<msg<<endl;
            if (writeN(cliSocket, msg.c_str(), msg.size()) <= 0) break;
        }

        else if (cmd == 't') {
            // Leer destinatario
            if (readN(cliSocket, buffer, 2) <= 0) break;
            int tamano_dest = atoi(buffer);

            if (readN(cliSocket, buffer, tamano_dest) <= 0) break;
            string destinatario(buffer, tamano_dest);

            // Leer mensaje
            if (readN(cliSocket, buffer, 3) <= 0) break;
            int tamano_msg = atoi(buffer);

            if (readN(cliSocket, buffer, tamano_msg) <= 0) break;
            string mensaje(buffer, tamano_msg);

            // Reenviar al destinatario
            lock_guard<mutex> lock(mapMutex);
            if (mapSockets.find(destinatario) != mapSockets.end()) {
                string protocolo = "T" + formatear_numeron((int)miNombre.size()) + miNombre +
                                   formatear_numerov((int)mensaje.size()) + mensaje;
                cout<<"Protocolo : "<<protocolo<<endl;

                if (writeN(mapSockets[destinatario], protocolo.c_str(), protocolo.size()) <= 0) {
                    cerr << "Error enviando a " << destinatario << endl;
                }
            } else {
                string error = "Usuario no encontrado.";
                writeN(cliSocket, error.c_str(), error.size());
            }
        }

        else if (cmd == 'n') {
            // Leer nombre
            if (readN(cliSocket, buffer, 2) <= 0) break;
            int tamano = atoi(buffer);

            if (readN(cliSocket, buffer, tamano) <= 0) break;
            string newUser(buffer, tamano);
            if(mapSockets.find(newUser)!=mapSockets.end()){
                string error = newUser + "is already taken";
                string protocolo = "E" + formatear_numerov((int)newUser.size()) + error;
                cout<<"Protocolo : "<<protocolo<<endl;
                writeN(cliSocket,protocolo.c_str(),(int)protocolo.size());
            }
            else{

                {
                    lock_guard<mutex> lock(mapMutex);
                    mapSockets[newUser] = cliSocket;
                }

            miNombre = newUser;
            cout << "Usuario " << miNombre << " añadido." << endl;
            }
        }

        else if (cmd == 'm') {
            // Leer mensaje broadcast
            if (readN(cliSocket, buffer, 3) <= 0) break;
            int tamano_msg = atoi(buffer);

            if (readN(cliSocket, buffer, tamano_msg) <= 0) break;
            string mensaje(buffer, tamano_msg);

            string enviador = miNombre;

            string mensaje_ = "M" + formatear_numeron((int)enviador.size()) + enviador +
                              formatear_numeron((int)mensaje.size()) + mensaje;
            cout<<"Protocolo : "<<mensaje<<endl;

            lock_guard<mutex> lock(mapMutex);
            for (const auto &[nombre, sock] : mapSockets) {
                if (sock != cliSocket) {
                    writeN(sock, mensaje_.c_str(), mensaje_.size());
                }
            }
        }
        else if (cmd == 'S') {
            // Leer destinatario
            if (readN(cliSocket, buffer, 2) <= 0) break;
            int tamano_dest = atoi(buffer);
        
            if (readN(cliSocket, buffer, tamano_dest) <= 0) break;
            string destinatario(buffer, tamano_dest);
        
            // Leer sala serializada
            if (readN(cliSocket, buffer, 3) <= 0) break;
            int tamano_datos = atoi(buffer);
        
            string data(tamano_datos, '\0');
            if (readN(cliSocket, &data[0], tamano_datos) <= 0) break;
        
            Sala sala = Sala::deserialize(data);
            cout << "[SERVIDOR] Sala recibida de " << miNombre
                 << " para " << destinatario << " (" << sala.descripcion << ")" << endl;
        
            // Reenviar al destinatario
            lock_guard<mutex> lock(mapMutex);
            if (mapSockets.find(destinatario) != mapSockets.end()) {
                string protocolo = "S" + formatear_numeron((int)miNombre.size()) + miNombre +
                                   formatear_numerov((int)data.size()) + data;
        
                cout << "Protocolo reenviado (Sala): " << protocolo << endl;
                writeN(mapSockets[destinatario], protocolo.c_str(), protocolo.size());
            }
        }
        
        
        else if(cmd=='X'){
            break;
        }

    }

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

        std::thread(readSocketThread, ClientFD).detach();
    }

    close(SocketFD);
    return 0;
}

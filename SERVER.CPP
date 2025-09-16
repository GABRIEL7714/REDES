#include <iostream>     
#include <thread>       
#include <cstring>      
#include <unistd.h>     
#include <arpa/inet.h>  
#include <sstream>      
#include <iomanip>      
#include <fstream>      
#include <sys/types.h>  
#include <sys/stat.h>   
#include <map>          
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
        else if (cmd == 'f') {
            // Destinatario
            if (readN(cliSocket, buffer, 2) <= 0) break;
            int tamano_dest = atoi(buffer);
        
            if (readN(cliSocket, buffer, tamano_dest) <= 0) break;
            string destinatario(buffer, tamano_dest);
        
            // Nombre archivo
            if (readN(cliSocket, buffer, 3) <= 0) break;
            int tamano_nombre = atoi(buffer);
        
            if (readN(cliSocket, buffer, tamano_nombre) <= 0) break;
            string nombre_archivo(buffer, tamano_nombre);
        
            // Tamaño archivo
            char sizebuf[11];
            if (readN(cliSocket, sizebuf, 10) <= 0) break;
            sizebuf[10] = '\0';
            int tamano_archivo = atoi(sizebuf);
        
            // Contenido
            string contenido(tamano_archivo, '\0');
            if (readN(cliSocket, &contenido[0], tamano_archivo) <= 0) break;
        
            // Guardar archivo en el servidor
            mkdir("archivos_recibidos", 0777);
            string ruta = "archivos_recibidos/" + nombre_archivo;
            ofstream out(ruta, ios::binary);
            out.write(contenido.data(), contenido.size());
            out.close();
        
            cout << "Archivo recibido de " << miNombre 
                 << " para " << destinatario 
                 << ": " << nombre_archivo 
                 << " (" << tamano_archivo << " bytes)" << endl;
        
            // Reenviar al destinatario
            lock_guard<mutex> lock(mapMutex);
            if (mapSockets.find(destinatario) != mapSockets.end()) {
                stringstream ss;
                ss << "f"
                   << formatear_numeron((int)miNombre.size()) << miNombre
                   << setw(3) << setfill('0') << (int)nombre_archivo.size() << nombre_archivo
                   << setw(10) << setfill('0') << tamano_archivo
                   << contenido;
        
                string protocolo = ss.str();
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

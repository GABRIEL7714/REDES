#include <iostream>     // cout, cerr
#include <thread>       // std::thread
#include <cstring>      // memset, strlen, memcpy
#include <unistd.h>     // read, write, close
#include <arpa/inet.h>  // socket, connect, sockaddr_in
#include <sstream>      // stringstream
#include <iomanip>      // setw, setfill
#include <fstream>      // ifstream, ofstream (manejo de archivos)
#include <sys/types.h>  // mkdir
#include <sys/stat.h>   // mkdir permisos
#include <map>          // std::map (mapSockets)
#include <mutex>        // std::mutex, std::lock_guard


using namespace std;

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

void recibir_mensajes(int socket_cliente) {
    char buffer[300];

    while (true) {
        int n = readN(socket_cliente, buffer, 1); // Leer tipo de mensaje
        if (n <= 0) {
            cout << "\nConexión cerrada o error al recibir." << endl;
            break;
        }

        char tipo = buffer[0];

        if (tipo == 'T') {
            // Mensaje privado
            if (readN(socket_cliente, buffer, 2) <= 0) break;
            int tamano_enviador = atoi(buffer);

            if (readN(socket_cliente, buffer, tamano_enviador) <= 0) break;
            string enviador(buffer, tamano_enviador);

            if (readN(socket_cliente, buffer, 3) <= 0) break;
            int tamano_mensaje = atoi(buffer);

            if (readN(socket_cliente, buffer, tamano_mensaje) <= 0) break;
            string mensaje(buffer, tamano_mensaje);

            cout << "\n[PRIVADO] " << enviador << " dice: " << mensaje << endl;
        }

        else if (tipo == 'M') {
            // Broadcast
            if (readN(socket_cliente, buffer, 2) <= 0) break;
            int tamano_enviador = atoi(buffer);

            if (readN(socket_cliente, buffer, tamano_enviador) <= 0) break;
            string enviador(buffer, tamano_enviador);

            if (readN(socket_cliente, buffer, 2) <= 0) break; // corregido a 2 digitos
            int tamano_mensaje = atoi(buffer);

            if (readN(socket_cliente, buffer, tamano_mensaje) <= 0) break;
            string mensaje(buffer, tamano_mensaje);

            cout << "\n[BROADCAST] " << enviador << " dice: " << mensaje << endl;
        }

        else if (tipo == 'L') {
            // Lista de usuarios
            if (readN(socket_cliente, buffer, 1) <= 0) break;
            int numero_clientes = buffer[0] - '0'; // tamaño pequeño

            string nombres = "";
            for (int i = 0; i < numero_clientes; i++) {
                if (readN(socket_cliente, buffer, 2) <= 0) break;
                int tamano_nombre = atoi(buffer);

                if (readN(socket_cliente, buffer, tamano_nombre) <= 0) break;
                string nombre(buffer, tamano_nombre);

                nombres += nombre + " ";
            }
            cout << "\nUsuarios conectados: " << nombres << endl;
        }
        else if(tipo=='E'){
            if (readN(socket_cliente, buffer, 3) <= 0) break;
            int tamano_mensaje = atoi(buffer);
            if (readN(socket_cliente, buffer, tamano_mensaje) <= 0) break;
            string mensaje = buffer;
            cout<<mensaje<<endl;
        }
        else if (tipo == 'f') {
            // Recibir emisor
            if (readN(socket_cliente, buffer, 2) <= 0) break;
            int tamano_emisor = atoi(buffer);
        
            if (readN(socket_cliente, buffer, tamano_emisor) <= 0) break;
            string emisor(buffer, tamano_emisor);
        
            // Nombre archivo
            if (readN(socket_cliente, buffer, 3) <= 0) break;
            int tamano_nombre = atoi(buffer);
        
            if (readN(socket_cliente, buffer, tamano_nombre) <= 0) break;
            string nombre_archivo(buffer, tamano_nombre);
        
            // Tamaño archivo
            char sizebuf[11];
            if (readN(socket_cliente, sizebuf, 10) <= 0) break;
            sizebuf[10] = '\0';
            int tamano_archivo = atoi(sizebuf);
        
            // Contenido
            string contenido(tamano_archivo, '\0');
            if (readN(socket_cliente, &contenido[0], tamano_archivo) <= 0) break;
        
            // Guardar
            mkdir("archivos_recibidos", 0777);
            string ruta = "archivos_recibidos/" + nombre_archivo;
            ofstream out(ruta, ios::binary);
            out.write(contenido.data(), contenido.size());
            out.close();
        
            cout << "\n[ARCHIVO] Recibido de " << emisor 
                 << " -> " << nombre_archivo 
                 << " (" << tamano_archivo << " bytes)" << endl;
        }
        
        else {
            cout << "\n[!] Mensaje desconocido: " << tipo << endl;
        }
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
    server_addr.sin_port = htons(45000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en connect");
        return 1;
    }

    thread t(recibir_mensajes, socket_fd);
    t.detach();

    while (true) {
        cout << "\n--- MENÚ ---" << endl;
        cout << "1. Añadir nuevo usuario" << endl;
        cout << "2. Enviar mensaje a otro usuario" << endl;
        cout << "3. Mostrar lista de usuarios" << endl;
        cout << "4. Enviar un mensaje a todos" << endl;
        cout << "5. Enviar archivos" << endl;
        cout << "6. Salir" << endl;
        cout << "Opción: ";

        int opcion;
        cin >> opcion;
        cin.ignore();

        if (opcion == 1) {
            string nombre;
            cout << "Nombre del nuevo usuario: ";
            getline(cin, nombre);

            string protocolo = "n" + formatear_numeron((int)nombre.size()) + nombre;
            cout<<"Protocolo : "<<protocolo<<endl;
            writeN(socket_fd, protocolo.c_str(), protocolo.size());
        }

        else if (opcion == 2) {
            string mensaje, receptor;
            cout << "Receptor: ";
            getline(cin, receptor);
            cout << "Mensaje: ";
            getline(cin, mensaje);

            string protocolo = "t" + formatear_numeron((int)receptor.size()) + receptor +
                               formatear_numerov((int)mensaje.size()) + mensaje;
            cout<<"Protocolo : "<<protocolo<<endl;
            writeN(socket_fd, protocolo.c_str(), protocolo.size());
        }

        else if (opcion == 3) {
            string protocolo = "l";
            cout<<"Protocolo : "<<protocolo<<endl;
            writeN(socket_fd, protocolo.c_str(), protocolo.size());
        }

        else if (opcion == 4) {
            string mensaje;
            cout << "Mensaje: ";
            getline(cin, mensaje);

            string protocolo = "m" + formatear_numerov((int)mensaje.size()) + mensaje;
            cout<<"Protocolo : "<<protocolo<<endl;
            writeN(socket_fd, protocolo.c_str(), protocolo.size());
        }

        else if (opcion == 5) {
            string receptor, ruta;
            cout << "Receptor: ";
            getline(cin, receptor);
            cout << "Ruta del archivo: ";
            getline(cin, ruta);

            ifstream file(ruta, ios::binary | ios::ate);
            if (!file) {
                cerr << "No se pudo abrir el archivo\n";
                continue;
            }

            streamsize size = file.tellg();
            file.seekg(0, ios::beg);

            string contenido(size, '\0');
            file.read(&contenido[0], size);

            // Nombre del archivo (sin ruta)
            string nombre = ruta.substr(ruta.find_last_of("/\\") + 1);

            // Protocolo
            stringstream ss;
            ss << "f"
            << formatear_numeron((int)receptor.size()) << receptor
            << setw(3) << setfill('0') << (int)nombre.size() << nombre
            << setw(10) << setfill('0') << size
            << contenido;

            string protocolo = ss.str();
            cout << "Protocolo archivo generado (truncado): " << protocolo.substr(0,50) << "..." << endl;

            writeN(socket_fd, protocolo.c_str(), protocolo.size());
        }

        else if (opcion == 6) {
            cout << "Cerrando cliente..." << endl;
            writeN(socket_fd,"X",1);
            break;
        }

        else {
            cout << "Opción inválida." << endl;
        }
    }

    close(socket_fd);
    return 0;
}

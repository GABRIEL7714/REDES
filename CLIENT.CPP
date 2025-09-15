#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>
#include <fstream>

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

string formatear_numerod(int numero) {
    stringstream ss;
    ss << setw(10) << setfill('0') << numero;
    return ss.str();
}

void recibir_mensajes(int socket_cliente) {
    char buffer[1024];
    while (true) {
        int bytes_recibidos = read(socket_cliente, buffer, 1);
        if (bytes_recibidos <= 0) {
            cout << "Conexión cerrada o error al recibir." << endl;
            break;
        }

        if (buffer[0] == 'T' || buffer[0] == 'M') {
            char contenido[512];
            int n = read(socket_cliente, contenido, 2);
            contenido[n] = '\0';
            int tamano_enviador = atoi(contenido);
            n = read(socket_cliente, contenido, tamano_enviador);
            string enviador = string(contenido, n);

            n = read(socket_cliente, contenido, 3);
            contenido[n] = '\0';
            int tamano_mensaje = atoi(contenido);
            n = read(socket_cliente, contenido, tamano_mensaje);
            string mensaje = string(contenido, n);

            cout << enviador << " dice: " << mensaje << endl;
        } 
        else if (buffer[0] == 'L') {
            char contenido[512];
            int n = read(socket_cliente, contenido, 2);
            contenido[n] = '\0';
            int numero_clientes = atoi(contenido);
            cout << "Usuarios conectados: " << numero_clientes << endl;

            for (int i = 0; i < numero_clientes; i++) {
                n = read(socket_cliente, contenido, 2);
                contenido[n] = '\0';
                int tamano_nombre = atoi(contenido);
                n = read(socket_cliente, contenido, tamano_nombre);
                string nombre = string(contenido, n);
                cout << " - " << nombre << endl;
            }
        }
        else if (buffer[0] == 'F') {  
            // recepción de archivo
            char contenido[1024];

            int n = read(socket_cliente, contenido, 2);
            contenido[n] = '\0';
            int tamano_nombre = atoi(contenido);
            n = read(socket_cliente, contenido, tamano_nombre);
            string nombre_archivo = string(contenido, n);

            n = read(socket_cliente, contenido, 3);
            contenido[n] = '\0';
            int tamano_archivo = atoi(contenido);

            string datos;
            datos.resize(tamano_archivo);
            int recibidos = 0;
            while (recibidos < tamano_archivo) {
                n = read(socket_cliente, &datos[recibidos], tamano_archivo - recibidos);
                recibidos += n;
            }

            ofstream out(nombre_archivo);
            out << datos;
            out.close();

            cout << "Archivo recibido y guardado como: " << nombre_archivo << endl;
        }
        else {
            cout << "Tipo de mensaje desconocido" << endl;
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
        cout << "4. Enviar mensaje a todos" << endl;
        cout << "5. Enviar archivo .txt" << endl;
        cout << "6. Salir" << endl;
        cout << "Elige una opción: ";

        int opcion;
        cin >> opcion;
        cin.ignore();

        if (opcion == 1) {
            string nombre;
            cout << "Nombre del nuevo usuario: ";
            getline(cin, nombre);

            string protocolo = "n" + formatear_numeron(nombre.size()) + nombre;
            write(socket_fd, protocolo.c_str(), protocolo.size());
        } 
        else if (opcion == 2) {
            string mensaje, receptor;
            cout << "Mensaje: ";
            getline(cin, mensaje);
            cout << "Receptor: ";
            getline(cin, receptor);

            string protocolo = "t" + formatear_numeron(receptor.size()) + receptor +
                               formatear_numerov(mensaje.size()) + mensaje;
            write(socket_fd, protocolo.c_str(), protocolo.size());
        }
        else if (opcion == 3) {
            string protocolo = "l";
            write(socket_fd, protocolo.c_str(), 1);
        }
        else if (opcion == 4) {
            string mensaje;
            cout << "Mensaje: ";
            getline(cin, mensaje);

            string protocolo = "m" + formatear_numerov(mensaje.size()) + mensaje;
            write(socket_fd, protocolo.c_str(), protocolo.size());
        }
        else if (opcion == 5) {
            string nombre_archivo,destinatario;
            cout << "Nombre del archivo .txt a enviar: ";
            getline(cin, nombre_archivo);

            ifstream in(nombre_archivo);
            if (!in.is_open()) {
                cout << "Error: no se pudo abrir el archivo." << endl;
                continue;
            }

            cout << "Nombre del destinatario :  ";
            getline(cin,destinatario);



            string contenido((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
            in.close();

            string protocolo = "f" +  formatear_numeron((int)destinatario.size()) + destinatario +   formatear_numerov(nombre_archivo.size()) + nombre_archivo +
                               formatear_numerod(contenido.size()) + contenido;

            write(socket_fd, protocolo.c_str(), protocolo.size());
            cout << "Archivo enviado: " << nombre_archivo << endl;
        }
        else if (opcion == 6) {
            break;
        }
    }

    close(socket_fd);
    return 0;
}

#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>

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

        // Comprobamos si el buffer empieza con un encabezado de protocolo (5 caracteres numéricos)
        if (isdigit(buffer[0]) && isdigit(buffer[1]) && isdigit(buffer[2]) && isdigit(buffer[3]) && isdigit(buffer[4])) {
            cout << "\n>> " << (buffer + 5) << endl;  // Imprimir después de los primeros 5 caracteres (el tamaño)
        } else {
            cout << "\n>> " << buffer << endl;  // Para otros mensajes, simplemente se muestra
        }
    }
}


string formatear_numero(int numero) {
    stringstream ss;
    ss << setw(5) << setfill('0') << numero;
    return ss.str();
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
        cout << "4. Salir" << endl;
        cout << "Elige una opción: ";

        int opcion;
        cin >> opcion;
        cin.ignore();

        if (opcion == 1) {
            string nombre;
            cout << "Nombre del nuevo usuario: ";
            getline(cin, nombre);

            string cuerpo = "N" + nombre;
            string protocolo = formatear_numero(cuerpo.length()) + cuerpo;
            write(socket_fd, protocolo.c_str(), protocolo.length());

        } 
        else if (opcion == 2) {
            string mensaje, receptor;
            cout << "Mensaje: ";
            cin.ignore(); // Limpiar buffer de entrada
            getline(cin, mensaje);
            cout << "Receptor: ";
            getline(cin, receptor);
        
            string cuerpo = mensaje + "#" + receptor;  // Mensaje y receptor separados por #
            int tam_total = cuerpo.length() + 1;       // +1 por el carácter 'M'
        
            char buffer[1024];
            sprintf(buffer, "%05dM%s", tam_total, cuerpo.c_str());
            write(socket_fd, buffer, strlen(buffer));
        }
        
        
        else if (opcion == 3) {
            string cuerpo = "L";
            string protocolo = formatear_numero(cuerpo.length()) + cuerpo;
            write(socket_fd, protocolo.c_str(), protocolo.length());

        } else if (opcion == 4) {
            cout << "Saliendo..." << endl;
            break;

        } else {
            cout << "Opción inválida. Intenta de nuevo." << endl;
        }
    }

    close(socket_fd);
    return 0;
}

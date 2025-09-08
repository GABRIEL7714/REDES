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
void recibir_mensajes(int socket_cliente) {
    char buffer[110];
    while (true) {
        int bytes_recibidos = read(socket_cliente, buffer, 1);  // Primero recibes los 5 bytes de tamaño
        if (bytes_recibidos <= 0) {
            cout << "Conexión cerrada o error al recibir." << endl;
            break;
        }

       
        if (buffer[0] == 'T') {
            char contenido[300];
            int n = read(socket_cliente,contenido,2);
            contenido[n] = '\0';
            int tamano_enviador = atoi(contenido);
             n = read(socket_cliente,contenido,tamano_enviador);
            string enviador=contenido;
            contenido[n] = '\0';

             n = read(socket_cliente,contenido,3);
            contenido[n] = '\0';

            int tamano_mensaje = atoi(contenido);
             n = read(socket_cliente,contenido,tamano_mensaje);
            contenido[n] = '\0';

            string mensaje = contenido;
            printf("%s dice : %s",enviador,mensaje);
;
        } 
        else if (buffer[0]  == 'M') {
            char contenido[300];
            int n = read(socket_cliente,contenido,2);
            contenido[n] = '\0';
            int tamano_enviador = atoi(contenido);
            n = read(socket_cliente,contenido,tamano_enviador);
            string enviador=contenido;
            contenido[n] = '\0';

            n = read(socket_cliente,contenido,3);
            contenido[n] = '\0';

            int tamano_mensaje = atoi(contenido);
            n = read(socket_cliente,contenido,tamano_mensaje);
            contenido[n] = '\0';

            string mensaje = contenido;
            printf("%s dice : %s",enviador,mensaje);

        }
        else if (buffer[0] == 'L') {

            string nombres="";
            char contenido[300];
            int n = read(socket_cliente,contenido,2);
            int numero_clientes = atoi(contenido);
            contenido[n] = '\0';
            for(int i=0;i<numero_clientes;i++){
                n = read(socket_cliente,contenido,2);
                int tamano_nombre = atoi(contenido);
                contenido[n] = '\0';
                n = read(socket_cliente,contenido,tamano_nombre);
                string nombre = contenido;
                contenido[n] = '\0';
                nombres += nombre + " ";

            } 
            cout << "Lista de usuarios conectados:\n" << nombres<< endl;


        } else {
            cout << "Tipo de mensaje desconocido: " << endl;
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
        cout << "5. Salir" << endl;
        cout << "Elige una opción: ";

        int opcion;
        cin >> opcion;
        cin.ignore();

        if (opcion == 1) {
            string nombre;
            cout << "Nombre del nuevo usuario: ";
            getline(cin, nombre);

            string protocolo = "n"+ formatear_numeron((int)nombre.size())+nombre;
            write(socket_fd, protocolo.c_str(), protocolo.size());

        } 
        else if (opcion == 2) {
            string mensaje, receptor;
            cout << "Mensaje: ";
            cin.ignore(); // Limpiar buffer de entrada
            getline(cin, mensaje);
            cout << "Receptor: ";
            getline(cin, receptor);
        
            string protocolo = "t" +formatear_numeron((int)receptor.size())+receptor + formatear_numerov((int)mensaje.size()) + mensaje;  // Mensaje y receptor separados por #
            write(socket_fd, protocolo.c_str(), (int)protocolo.size());
        }
        
        else if (opcion == 3) {
            string  protocolo = "l";
            write(socket_fd, protocolo.c_str(), 1);

        }
        else if(opcion ==4){
            string mensaje = "";
            cout<<"Mensaje : ";
            getline(cin,mensaje);

            string protocolo = "m" +formatear_numerov((int)(mensaje.size())) + mensaje;
            write(socket_fd,protocolo.c_str(),(int)protocolo.size());
        
        }
        else if (opcion == 5) {
            break;
        } 
    }

    close(socket_fd);
    return 0;
}

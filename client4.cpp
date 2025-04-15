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

string calcularSHA256(const string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)data.c_str(), data.size(), hash);
  
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    return ss.str();
  }
void recibir_mensajes(int socket_cliente) {
    char buffer[110];
    while (true) {
        int bytes_recibidos = recv(socket_cliente, buffer, 5, 0);  // Primero recibes los 5 bytes de tamaño
        if (bytes_recibidos <= 0) {
            cout << "Conexión cerrada o error al recibir." << endl;
            break;
        }

        buffer[bytes_recibidos] = '\0';
        int tamano = atoi(buffer);  // conviertes a entero

        bytes_recibidos = recv(socket_cliente, buffer, tamano, 0);  // Ahora recibes el resto
        if (bytes_recibidos <= 0) {
            cout << "Conexión cerrada o error al recibir contenido." << endl;
            break;
        }

        buffer[bytes_recibidos] = '\0';
        char tipo = buffer[0];

        if (tipo == 'F') {
            string contenido(buffer + 1);
            string nombre_archivo = "archivo_recibido.txt";

            mkdir("archivos_recibidos", 0777);
            string ruta = "archivos_recibidos/" + nombre_archivo;
            ofstream out(ruta, ios::binary);
            out << contenido;
            out.close();
            cout << "Archivo recibido y guardado en: " << ruta << endl;

        } else if (tipo == 'M') {
            string contenido(buffer + 1);
            cout << "Mensaje recibido: " << contenido << endl;

        } else if (tipo == 'L') {
            string contenido(buffer + 1);
            cout << "Lista de usuarios conectados:\n" << contenido << endl;

        } else {
            cout << "Tipo de mensaje desconocido: " << tipo << endl;
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
        cout << "4. Enviar un archivo" << endl;
        cout << "5. Broadcast" << endl;
        cout << "6. Salir" << endl;
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

        }
        else if(opcion ==4){
            string receptor, nombre_archivo;
            cout << "Nombre del receptor (5 caracteres): ";
            getline(cin, receptor);
            if (receptor.length() < 5) receptor.insert(receptor.begin(), 5 - receptor.length(), '0');
        
            cout << "Nombre del archivo a enviar: ";
            getline(cin, nombre_archivo);
        
            FILE* archivo = fopen(nombre_archivo.c_str(), "rb");
            if (!archivo) {
                cout << "No se pudo abrir el archivo." << endl;
                continue;
            }
        
            char contenido[101] = {0};
            fread(contenido, 1, 100, archivo);  // Leemos hasta 100 bytes
            fclose(archivo);
        
            string mensaje = "F" + receptor + string(contenido);  // F + receptor + contenido
            string protocolo = formatear_numero(mensaje.length()) + mensaje;
        
            write(socket_fd, protocolo.c_str(), protocolo.length());
            cout << "Archivo enviado con éxito.\n";
        }
        else if(opcion==5){
            string mensaje;
            cout << "Mensaje a enviar a todos: ";
            cin.ignore();
            getline(cin, mensaje);
        
            string cuerpo = "B" + mensaje; // 'B' de Broadcast
            string protocolo = formatear_numero(cuerpo.length()) + cuerpo;
            write(socket_fd, protocolo.c_str(), protocolo.length());
        }
        else if (opcion == 6) {
            cout << "Saliendo..." << endl;
            string cuerpo = "Q";
            string protocolo = formatear_numero(cuerpo.length()) + cuerpo;  // 00001Q
            write(socket_fd, protocolo.c_str(), protocolo.length());
            break;
        } 
    }

    close(socket_fd);
    return 0;
}

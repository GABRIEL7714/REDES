#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <iostream> // std::cout
#include <thread>   // std::thread, std::this_thread::sleep_for
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
using namespace std;

std::map<string,int> mapSockets;  

string calcularSHA256(const string& data) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char*)data.c_str(), data.size(), hash);

  stringstream ss;
  for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
      ss << hex << setw(2) << setfill('0') << (int)hash[i];
  return ss.str();
}
void readSocketThread(int cliSocket, string nickname)
{
  char buffer[300];
  int n, tamano;
  do
  {
    n = read(cliSocket, buffer, 5);
    buffer[n]='\0';
    tamano = atoi(buffer);
    n = read(cliSocket,buffer,1);
    
    if (buffer[0] == 'L') {
      // Listar todos los usuarios
      string msg;
      for (auto it = mapSockets.cbegin(); it != mapSockets.cend(); ++it)
          msg = msg + (*it).first + "\n";  
      sprintf(buffer, "%05d%s", int(msg.length())+1, msg.c_str());
      write(cliSocket, buffer, msg.length() + 1 + 5);
    }
    else if (buffer[0] == 'M') {
      char contenido[300];
      n = read(cliSocket, contenido, tamano - 1);
      contenido[n] = '\0';
    
      // Dividir el mensaje y el receptor
      string full_msg(contenido);
      size_t sep = full_msg.find('#');  // Usa # como separador
    
      if (sep != string::npos) {
        string mensaje = full_msg.substr(0, sep);
        string receptor = full_msg.substr(sep + 1);
    
        if (mapSockets.find(receptor) != mapSockets.end()) {
          write(mapSockets[receptor], mensaje.c_str(), mensaje.length() + 1);
        } else {
          string error = "Usuario no encontrado.";
          write(cliSocket, error.c_str(), error.length() + 1);
        }
      }
    }
    
    else if (buffer[0] == 'N') {
      // Añadir nuevo usuario
      n = read(cliSocket, buffer, tamano - 1);
      buffer[n] = '\0';
      
      string newUser(buffer);
      mapSockets[newUser] = cliSocket; 
      sprintf(buffer, "Usuario %s añadido", newUser.c_str());
      write(cliSocket, buffer, strlen(buffer) + 1);  
    }

    else if (buffer[0] == 'F') {
      char contenido[200];
      n = read(cliSocket, contenido, tamano - 1);
      contenido[n] = '\0';
  
      string data(contenido);
      string destinatario = data.substr(0, 5);
      string archivo = data.substr(5);  // contenido del archivo
  
      if (mapSockets.find(destinatario) != mapSockets.end()) {
          string envio = "F" + archivo;
          envio.insert(envio.end(), archivo.begin(), archivo.end());
          string protocolo = to_string(envio.length());
          protocolo = string(5 - protocolo.length(), '0') + protocolo;
          write(mapSockets[destinatario], (protocolo + envio).c_str(), protocolo.length() + envio.length());
      } else {
          string error = "Usuario destino no encontrado.";
          write(cliSocket, error.c_str(), error.length() + 1);
      }
  }
  else if (buffer[0] == 'B') {
    char contenido[300];
    n = read(cliSocket, contenido, tamano - 1);
    contenido[n] = '\0';
    string mensaje = "M" + string(contenido);  // Reutilizamos 'M' como tipo de mensaje para todos

    for (const auto& [nombre, sock] : mapSockets) {
        if (sock != cliSocket) { // Opcional: no se lo envía a sí mismo
            string protocolo = to_string(mensaje.length());
            protocolo = string(5 - protocolo.length(), '0') + protocolo;
            write(sock, (protocolo + mensaje).c_str(), protocolo.length() + mensaje.length());
        }
    }
}
else if (buffer[0] == 'Q') {
  cout << "Se recibió señal de salida (Q). Terminando servidor..." << endl;
  exit(0);  // Termina el servidor inmediatamente
}


  

  } while (strncmp(buffer, "exit", 4) != 0);  
  shutdown(cliSocket, SHUT_RDWR);
  close(cliSocket);
  mapSockets.erase(nickname);  
}
int main(void)
{
  struct sockaddr_in stSockAddr;
  int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  char buffer[256];
  int n;

  if (-1 == SocketFD)
  {
    perror("can not create socket");
    exit(EXIT_FAILURE);
  }

  memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

  stSockAddr.sin_family = AF_INET;
  stSockAddr.sin_port = htons(45000);
  stSockAddr.sin_addr.s_addr = INADDR_ANY;

  if (-1 == bind(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
  {
    perror("error bind failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }

  if (-1 == listen(SocketFD, 10))
  {
    perror("error listen failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }

  for (;;)
  {
    int ClientFD = accept(SocketFD, NULL, NULL);

    if (0 > ClientFD)
    {
      perror("error accept failed");
      close(SocketFD);
      exit(EXIT_FAILURE);
    }

    n = read(ClientFD, buffer, 5); 
    buffer[n] = '\0'; 
    int tamano = atoi(buffer);
    n = read(ClientFD, buffer, 1);
    
    if (buffer[0] == 'N') {
      // Añadir nuevo usuario
      n = read(ClientFD, buffer, tamano - 1);
      buffer[n] = '\0';
      mapSockets[buffer] = ClientFD;
      printf("Nuevo cliente añadido: %s\n", buffer);
      std::thread(readSocketThread, ClientFD, buffer).detach();
    } 
    else {
      shutdown(ClientFD, SHUT_RDWR);
      close(ClientFD);
    }
  }

  close(SocketFD);
  return 0;
}

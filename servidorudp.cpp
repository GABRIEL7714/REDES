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
#include <iostream> // std::cout
#include <thread>   // std::thread, std::this_thread::sleep_for
#include <map>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unistd.h>

using namespace std;

std::unordered_map<std::string, sockaddr_in> mapaAddr;
std::unordered_map<std::string, std::vector<std::string>> clientePkgs;

std::vector<std::string> partir(std::string msg, int tamMax)
{
  std::vector<std::string> partes;
  int len = msg.size();

  for (int i = 0; i < len; i += tamMax)
  {
    partes.push_back(msg.substr(i, tamMax));
  }

  return partes;
}

struct Partida
{
  char tablero[9] = {'_', '_', '_', '_', '_', '_', '_', '_', '_'};
  string jugadorO; // quien empieza
  string jugadorX;
  vector<string> espectadores;
  char turno = 'O';
  bool activa = false;
} partida;

string jugadorEnEspera; // primer J que llego y espera rival

void enviarM(int sock, const sockaddr_in &dest, const std::string &msg);
void enviarX_aTodos(int sock);
void enviarT(int sock, const std::string &nick, char simbolo);
void finalizarPartida(char resultado); // 'W','L','E'
bool ganador(char s);
bool tableroLleno();

void procesarMensajes(std::vector<std::string> pkgs, std::string nickname, int sock, char tipo)
{
  char buffer[500];

  // MENSAJE l
  if (tipo == 'L')
  {
    int sizeFijo = 6;
    int tamMax = 500 - sizeFijo;

    std::string mensaje;
    std::string coma = "";
    for (auto it = mapaAddr.cbegin(); it != mapaAddr.cend(); ++it)
    {
      if ((*it).first != nickname)
      {
        mensaje += coma + (*it).first;
        coma = ",";
      }
    }
    sockaddr_in dest = mapaAddr[nickname];

    if (mensaje.length() > tamMax)
    {
      std::vector<std::string> piezas = partir(mensaje, tamMax);
      int seq = 1;
      int totalPkg = piezas.size();
      for (int i = 0; i < totalPkg; i++)
      {
        std::memset(buffer, '#', 500);
        sprintf(buffer, "%02d%02d%05dl%s\0", seq++, totalPkg, (int)piezas[i].length() + 1, piezas[i].c_str());
        sendto(sock, buffer, 500, 0,
               (sockaddr *)&dest, sizeof(dest));
      }
    }
    else
    {
      std::memset(buffer, '#', 500);
      sprintf(buffer, "0001%05dl%s\0", (int)mensaje.length() + 1, mensaje.c_str());
      sendto(sock, buffer, 500, 0,
             (sockaddr *)&dest, sizeof(dest));
    }
  }

  // MENSAJE m
  else if (tipo == 'M')
  {
    int sizeFijo = 20 + nickname.length(); // 2+2+5+1+5+5+tamRem
    int tamMax = 500 - sizeFijo;           // espacio para mensaje

    // destinatario
    int tamMsg1 = std::stoi(pkgs[0].substr(10, 5));
    int tamDest = std::stoi(pkgs[0].substr(15 + tamMsg1, 5));
    std::string destino = pkgs[0].substr(15 + tamMsg1 + 5, tamDest);

    std::cout << "Destino: " << destino.c_str() << "\n";

    // reconstruir mensaje
    std::string mensaje;
    for (size_t i = 0; i < pkgs.size(); ++i) // en orden secuencia
    {
      int t = std::stoi(pkgs[i].substr(10, 5)); // tamMsg
      mensaje += pkgs[i].substr(15, t);
    }
    std::cout << "Mensaje: " << mensaje.c_str() << "\n";

    sockaddr_in dest = mapaAddr[destino];

    // partir si excede tamMax
    std::vector<std::string> piezas = (mensaje.length() > tamMax)
                                          ? partir(mensaje, tamMax) // func que parte en trozos <=tamMax
                                          : std::vector<std::string>{mensaje};

    int totalPkg = piezas.size();
    int seq = 1;

    for (auto &trozo : piezas)
    {
      // datagrama completo de 500B
      std::memset(buffer, '#', 500); // relleno

      // secuencia y total  (bytes 0-3)
      std::sprintf(buffer, "%02d%02d", (seq == totalPkg ? 0 : seq), totalPkg);
      int totalSize = 1 + 5 + trozo.length() + 5 + nickname.length();

      // empezamos luego de seq y total
      int offset = 4;

      // tamaño total
      std::sprintf(buffer + offset, "%05d", totalSize);
      offset += 5;
      buffer[offset++] = 'm';

      // tamaño mensaje
      std::sprintf(buffer + offset, "%05d", (int)trozo.length());
      offset += 5;
      std::memcpy(buffer + offset, trozo.c_str(), trozo.length());
      offset += trozo.length();

      // tamaño nickname
      std::sprintf(buffer + offset, "%05d", (int)nickname.length());
      offset += 5;
      std::memcpy(buffer + offset, nickname.c_str(), nickname.length());

      // lo que resta es #

      // enviar 500B
      sendto(sock, buffer, 500, 0, (sockaddr *)&dest, sizeof(dest));
      ++seq;
    }
  }

  // MENSAJE b
  else if (tipo == 'B')
  {
    int sizeFijo = 20 + nickname.length(); // 2+2+5+1+5+5+tamRem
    int tamMax = 500 - sizeFijo;           // espacio para mensaje

    // reconstruir mensaje
    std::string mensaje;
    for (size_t i = 0; i < pkgs.size(); ++i) // en orden secuencia
    {
      int t = std::stoi(pkgs[i].substr(10, 5)); // tamMsg
      mensaje += pkgs[i].substr(15, t);
    }

    // partir si excede tamMax
    std::vector<std::string> piezas = (mensaje.length() > tamMax)
                                          ? partir(mensaje, tamMax) // func que parte en trozos <=tamMax
                                          : std::vector<std::string>{mensaje};

    int totalPkg = piezas.size();
    int seq = 1;

    for (auto &trozo : piezas)
    {
      // datagrama completo de 500B
      std::memset(buffer, '#', 500); // relleno

      // secuencia y total  (bytes 0-3)
      std::sprintf(buffer, "%02d%02d", (seq == totalPkg ? 0 : seq), totalPkg);
      int totalSize = 1 + 5 + trozo.length() + 5 + nickname.length();

      // empezamos luego de seq y total
      int offset = 4;

      // tamaño total
      std::sprintf(buffer + offset, "%05d", totalSize);
      offset += 5;
      buffer[offset++] = 'b';

      // tamaño mensaje
      std::sprintf(buffer + offset, "%05d", (int)trozo.length());
      offset += 5;
      std::memcpy(buffer + offset, trozo.c_str(), trozo.length());
      offset += trozo.length();

      // tamaño nickname
      std::sprintf(buffer + offset, "%05d", (int)nickname.length());
      offset += 5;
      std::memcpy(buffer + offset, nickname.c_str(), nickname.length());

      // lo que resta es #

      // enviar 500B
      for (const auto &par : mapaAddr) // par.first = nick
      {
        if (par.first == nickname)
          continue; // saltar al remitente

        const sockaddr_in &dest = par.second; // direccion
        sendto(sock, buffer, 500, 0, (const sockaddr *)&dest, sizeof(dest));
      }
      ++seq;
    }
  }

  // MENSAJE f
  else if (tipo == 'F')
  {
    // printf("\nEntrando a mensaje F (archivo)\n");
    // destinatario, filename y hash
    int tamDest = std::stoi(pkgs[0].substr(10, 5));                                        // 5B tamDest
    std::string destino = pkgs[0].substr(15, tamDest);                                     // destinatario
    int tamFileName = std::stoi(pkgs[0].substr(15 + tamDest, 100));                        // 100B tamFN
    std::string fileName = pkgs[0].substr(15 + tamDest + 100, tamFileName);                // filename
    long tamFile = std::stol(pkgs[0].substr(15 + tamDest + 100 + tamFileName, 18));        // 18B tamFile
    std::string hash = pkgs[0].substr(15 + tamDest + 100 + tamFileName + 18 + tamFile, 5); // hash

    int sizeFijo = 138 + nickname.length() + fileName.length(); // 4(seq/tot)+5(tamTot)+1+5+5+100+18+lenFN+5(hash)
    int tamMax = 500 - sizeFijo;                                // espacio para archivo

    // reconstruir archivo
    std::string archivo;
    for (size_t i = 0; i < pkgs.size(); ++i) // en orden secuencia
    {
      long lenTrozo = std::stol(pkgs[i].substr(15 + tamDest + 100 + tamFileName, 18)); // tamFile_i
      size_t posDatos = 15 + tamDest + 100 + tamFileName + 18;
      archivo += pkgs[i].substr(posDatos, lenTrozo); // trozo_i
    }

    // partir si excede tamMax
    std::vector<std::string> piezas = (archivo.length() > tamMax)
                                          ? partir(archivo, tamMax) // func que parte en trozos <=tamMax
                                          : std::vector<std::string>{archivo};

    int totalPkg = piezas.size();
    int seq = 1;

    for (auto &trozo : piezas)
    {
      // datagrama completo de 500B
      std::memset(buffer, '#', 500); // relleno

      // secuencia y total  (bytes 0-3)
      std::sprintf(buffer, "%02d%02d", (seq == totalPkg ? 0 : seq), totalPkg);

      /* ---- tamaño total del paquete lógico ------------------ */
      int totalSize = 1                         /* tipo 'f'     */
                      + 5 + nickname.length()   /* rem           */
                      + 100 + fileName.length() /* nombre file   */
                      + 18 + trozo.length()     /* trozo archivo */
                      + 5;                      /* hash          */

      // empezamos luego de seq y total
      int offset = 4;

      // tamaño total
      std::sprintf(buffer + offset, "%05d", totalSize);
      offset += 5;
      buffer[offset++] = 'f';

      // tamaño remitente
      std::sprintf(buffer + offset, "%05d", (int)nickname.length());
      offset += 5;
      std::memcpy(buffer + offset, nickname.c_str(), nickname.length());
      offset += nickname.length();

      // tamaño filename (100 B en ceros a la izquierda)
      std::sprintf(buffer + offset, "%0100d", (int)fileName.length());
      offset += 100;
      std::memcpy(buffer + offset, fileName.c_str(), fileName.length());
      offset += fileName.length();

      // tamaño de ESTE trozo de archivo (18 B)
      std::sprintf(buffer + offset, "%018d", (int)trozo.length());
      offset += 18;
      std::memcpy(buffer + offset, trozo.c_str(), trozo.length());
      offset += trozo.length();

      // hash (5 B)
      std::memcpy(buffer + offset, hash.c_str(), 5);

      // lo que resta es #

      sockaddr_in dest = mapaAddr[destino];
      // enviar 500B
      sendto(sock, buffer, 500, 0,
             (sockaddr *)&dest, sizeof(dest));
      ++seq;
    }
  }

  // MENSAJE Q
  else if (tipo == 'Q')
  {
    printf("\nEl cliente %s ha salido del chat\n", nickname.c_str());
    mapaAddr.erase(nickname);
    return;
  }

  // MENSAJE J
  else if (tipo == 'J')
  {
    if (!partida.activa && jugadorEnEspera.empty()) // 1er jugador
    {
      jugadorEnEspera = nickname;
      sockaddr_in dest = mapaAddr[nickname]; // dirección del cliente
      enviarM(sock, dest, "wait for player");
    }

    else if (!partida.activa && !jugadorEnEspera.empty())
    { // 2do jugador
      partida.activa = true;
      partida.jugadorO = jugadorEnEspera;
      partida.jugadorX = nickname;
      jugadorEnEspera = "";

      // m “inicio” a ambos
      enviarM(sock, mapaAddr[partida.jugadorO], "inicio");
      enviarM(sock, mapaAddr[partida.jugadorX], "inicio");

      enviarX_aTodos(sock);                 // X tablero vacío
      enviarT(sock, partida.jugadorO, 'O'); // primer turno
    }
    else
    { // espectador
      enviarM(sock, mapaAddr[nickname], "do you want to see?");
    }
  }

  // MENSAJE V
  else if (tipo == 'V')
  {
    if (partida.activa) // agregamos espectador
      partida.espectadores.push_back(nickname);

    char pkt[500];
    std::memset(pkt, '#', 500); // relleno con ‘#’

    // mensaje unico
    std::memcpy(pkt, "0001", 4); // 2B seq + 2B tot

    // total
    std::memcpy(pkt + 4, "00010", 5); // 5 bytes

    // tipo x (tablero)
    pkt[9] = 'X'; // byte 9
    std::memcpy(pkt + 10, partida.tablero, 9);

    // 500B
    sockaddr_in dest = mapaAddr[nickname];
    sendto(sock, pkt, 500, 0,
           (sockaddr *)&dest, sizeof(dest));
  }

  // MENSAJE P
  else if (tipo == 'P')
  {
    // pos y simbolo
    char pos = pkgs[0][10];
    char simb = pkgs[0][11];

    int idx = pos - '1'; // 0-8
    if (idx < 0 || idx > 8 || partida.tablero[idx] != '_')
    {
      /// Error 6
      char errPkt[500];
      std::memset(errPkt, '#', 500); // relleno

      // unico
      std::memcpy(errPkt, "0001", 4);

      // tam total
      std::memcpy(errPkt + 4, "00018", 5);

      int off = 9;
      errPkt[off++] = 'E';
      errPkt[off++] = '6';
      // tam mensaje
      std::memcpy(errPkt + off, "00016", 5);
      off += 5; // tamMensaje
      std::memcpy(errPkt + off, "Posicion ocupada", 16);
      // envio de 500B
      const sockaddr_in &cliAddr = mapaAddr[nickname];

      sendto(sock, errPkt, 500, 0,
             (const sockaddr *)&cliAddr, sizeof(cliAddr));
      return;
    }

    // actualizar tablero
    partida.tablero[idx] = simb;

    // ganador o empate
    if (ganador(simb))
    {
      enviarX_aTodos(sock); // tablero final
      sleep(0.5);
    
      // mensajes O
      auto enviaResultado = [&](const std::string &nick, char res)
      {
        char pkt[500];
        std::memset(pkt, '#', 500);
        std::memcpy(pkt, "0001", 4);      // unico paquete
        std::memcpy(pkt + 4, "00002", 5); // tamTot = 2
        pkt[9] = 'O';
        pkt[10] = res;
        sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[nick], sizeof(sockaddr_in));
      };

      enviaResultado(partida.jugadorO, (simb == 'O') ? 'W' : 'L');
      enviaResultado(partida.jugadorX, (simb == 'X') ? 'W' : 'L');
      for (auto &esp : partida.espectadores)
        enviaResultado(esp, 'E'); // espectadores

      partida = Partida(); // reset
    }
    else if (tableroLleno()) // empate
    {
      enviarX_aTodos(sock);

      char pkt[500];
      std::memset(pkt, '#', 500);
      std::memcpy(pkt, "0001", 4);
      std::memcpy(pkt + 4, "00002", 5);
      pkt[9] = 'O';
      pkt[10] = 'E';

      for (auto nick : {partida.jugadorO, partida.jugadorX})
        sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[nick], sizeof(sockaddr_in));
      for (auto &esp : partida.espectadores)
        sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[esp], sizeof(sockaddr_in));

      partida = Partida();
    }
    else
    {
      enviarX_aTodos(sock);
      partida.turno = (partida.turno == 'O') ? 'X' : 'O';
      const std::string &prox =
          (partida.turno == 'O') ? partida.jugadorO
                                 : partida.jugadorX;
      enviarT(sock, prox, partida.turno);
    }
  }
}

//----func para tic tac toe----------------------

void enviarM(int sock,                // socket UDP
             const sockaddr_in &dest, // dirección destino
             const std::string &msg)  // texto
{
  const std::string remitente = "servidor"; // fijo
  const int lenMsg = (int)msg.size();
  const int lenRem = (int)remitente.size();

  int tamTot = 1 + 5 + lenMsg + 5 + lenRem; // tipo + tamMsg + msg + tamRem + rem
  char buffer[500];
  std::memset(buffer, '#', 500); // padding

  // unico paquete
  std::memcpy(buffer, "0001", 4);

  // tamaño total
  std::sprintf(buffer + 4, "%05d", tamTot);
  int off = 9;
  buffer[off++] = 'm';

  // tamaño mensaje
  std::sprintf(buffer + off, "%05d", lenMsg);
  off += 5;
  std::memcpy(buffer + off, msg.c_str(), lenMsg);
  off += lenMsg;

  // tamaño remitente
  std::sprintf(buffer + off, "%05d", lenRem);
  off += 5;
  std::memcpy(buffer + off, remitente.c_str(), lenRem);
  // resto #
  // envia 500b
  if (sendto(sock, buffer, 500, 0,
             (const sockaddr *)&dest, sizeof(dest)) == -1)
    perror("sendto");
}

void enviarX_aTodos(int sock)
{
  char pkt[500];
  std::memset(pkt, '#', 500); // padding ‘#’

  // mensaje unico
  std::memcpy(pkt, "0001", 4); // único paquete

  // tamaño total
  std::memcpy(pkt + 4, "00010", 5); // 5 bytes

  // tipo y tablero
  pkt[9] = 'X';                              // byte 9
  std::memcpy(pkt + 10, partida.tablero, 9); // 9 bytes matriz
  for (int i = 0; pkt[i] != '#' && pkt[i] != '\0'; ++i)
  {
    std::cout << pkt[i];
  }
  std::cout << std::endl;

  // envio a jugadores
  for (auto nick : {partida.jugadorO, partida.jugadorX})
    if (!nick.empty())
      sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[nick], sizeof(sockaddr_in));

  // envio espectadores
  for (auto &esp : partida.espectadores)
    sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[esp], sizeof(sockaddr_in));
}

void enviarT(int sock, const std::string &nick, char simbolo)
{
  char pkt[500];
  std::memset(pkt, '#', 500); // padding
  // mensaje unico
  std::memcpy(pkt, "0001", 4);

  // tamao total
  std::memcpy(pkt + 4, "00002", 5);

  // tipo y simbolo
  pkt[9] = 'T';
  pkt[10] = simbolo; // 'O' o 'X'

  for (int i = 0; pkt[i] != '#' && pkt[i] != '\0'; ++i)
  {
    std::cout << pkt[i];
  }
  std::cout << std::endl;

  sendto(sock, pkt, 500, 0, (sockaddr *)&mapaAddr[nick], sizeof(sockaddr_in));
}

bool linea(int a, int b, int c, char s)
{
  return partida.tablero[a] == s && partida.tablero[b] == s && partida.tablero[c] == s;
}
bool ganador(char s)
{
  return linea(0, 1, 2, s) || linea(3, 4, 5, s) || linea(6, 7, 8, s) ||
         linea(0, 3, 6, s) || linea(1, 4, 7, s) || linea(2, 5, 8, s) ||
         linea(0, 4, 8, s) || linea(2, 4, 6, s);
}
bool tableroLleno()
{
  for (char c : partida.tablero)
    if (c == '_')
      return false;
  return true;
}

//----------------------------------------------

int main(void)
{
  char buffer[500];

  int sock;
  int addr_len, bytes_read;
  struct sockaddr_in server_addr, client_addr;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("Socket");
    exit(1);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(5000);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  bzero(&(server_addr.sin_zero), 8);

  if (bind(sock, (struct sockaddr *)&server_addr,
           sizeof(struct sockaddr)) == -1)
  {
    perror("Bind");
    exit(1);
  }

  addr_len = sizeof(struct sockaddr);
  sockaddr_in cli;
  socklen_t cliLen = sizeof(cli);

  for (;;)
  {
    // recibir 500b
    int n = recvfrom(sock, buffer, 500, 0,
                     (sockaddr *)&cli, &cliLen);
    if (n != 500)
    { // cualquier cosa ≠ 500 se descarta
      std::cout << "ERROR size mensaje\n";
      continue;
    }

    //
    char clave[32];
    sprintf(clave, "%s:%d",
            inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

    /* --- copiamos SIEMPRE el datagrama entero a un std::string -- */
    std::string pkg(buffer, 500); // incluye posibles '\0'

    // Registro
    if (pkg[9] == 'N')
    {
      std::cout << pkg.c_str() << "\n";
      int tamNick = std::stoi(pkg.substr(4, 5)) - 1;
      std::string nick = pkg.substr(10, tamNick);
      mapaAddr[nick] = cli;
      std::cout << "El cliente " << nick.c_str() << " se ha unido al chat" << "\n"; // registramos su addr
      continue;                                                                     // listo
    }

    // buscar nick del remitente
    std::string nick;
    for (auto &kv : mapaAddr)
      if (memcmp(&kv.second, &cli, sizeof(cli)) == 0)
      {
        nick = kv.first;
        break;
      }
    if (nick.empty())
      continue; // aún no registrado, descartar

    // seq y total
    int seq = std::stoi(std::string(pkg, 0, 2)); // “00”, “01”, …
    int tot = std::stoi(std::string(pkg, 2, 2)); // “01”…“99”
    int idx = (seq == 0) ? tot - 1 : seq - 1;

    // guardar
    auto &vc = clientePkgs[clave]; // std::vector<std::string>
    if (vc.empty())
      vc.resize(tot); // primer fragmento → redimensiona

    if (idx >= tot)
      continue; // fragmento corrupto
    if (!vc[idx].empty())
      continue; // duplicado ignorar

    vc[idx] = std::move(pkg); // guardamos esta parte

    // verificar si se tiene todo
    bool completo = true;
    for (auto &p : vc)
      if (p.empty())
      {
        completo = false;
        break;
      }
    // std::cout << completo << "\n";

    if (completo)
    {
      char tipo = vc[0][9]; // byte 9 del primer fragmento
      std::vector<std::string> partes = std::move(vc); // copiamos / movemos
      clientePkgs.erase(clave);                        // limpiamos para el siguiente msg

      std::thread(procesarMensajes, std::move(partes),
                  nick, sock, tipo)
          .detach();
    }
  }
  /* nunca llegamos aquí */
  close(sock);
  return 0;
}

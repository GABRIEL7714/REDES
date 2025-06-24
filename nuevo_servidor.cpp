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
#include <unistd.h>

using namespace std;

std::map<std::string, sockaddr_in> clientesConectados;
std::unordered_map<std::string, std::vector<std::string>> buffersPaquetesCliente;

std::vector<std::string> fragmentarDatos(std::string mensajeFuente, int tamanoMaxRebanada) {
    std::vector<std::string> rebanadas;
    int longitudMensaje = mensajeFuente.size();

    for (int i = 0; i < longitudMensaje; i += tamanoMaxRebanada) {
        rebanadas.push_back(mensajeFuente.substr(i, tamanoMaxRebanada));
    }
    return rebanadas;
}

struct SesionJuego {
    char tableroJuego[9] = {'_', '_', '_', '_', '_', '_', '_', '_', '_'};
    string jugadorCirculo;
    string jugadorCruz;
    vector<string> observadoresJuego;
    char simboloJugadorActual = 'O';
    bool estaActiva = false;
} juegoActualTicTacToe;

string jugadorEnEspera;

void enviarMensajePrivado(int idSocket, const sockaddr_in &direccionDestino, const std::string &textoMensaje);
void enviarTableroATodos(int idSocket);
void notificarTurno(int idSocket, const std::string &aliasJugador, char simboloAnotar);
bool verificarCondicionVictoria(char simboloJugador);
bool estaTableroCompleto();

void procesarDatosCliente(std::vector<std::string> fragmentosRecibidos, std::string aliasCliente, int socketServidor, char tipoPaqueteDatos) {
    char bufferSalida[500];

    if (tipoPaqueteDatos == 'L') {
        int sobrecargaFija = 6;
        int tamanoMaximoCargaUtil = 500 - sobrecargaFija;

        std::string mensajeListaUsuarios;
        std::string separador = "";
        for (auto iteradorCliente = clientesConectados.cbegin(); iteradorCliente != clientesConectados.cend(); ++iteradorCliente) {
            if ((*iteradorCliente).first != aliasCliente) {
                mensajeListaUsuarios += separador + (*iteradorCliente).first;
                separador = ",";
            }
        }
        sockaddr_in destinoCliente = clientesConectados[aliasCliente];

        if (mensajeListaUsuarios.length() > tamanoMaximoCargaUtil) {
            std::vector<std::string> rebanadasMensaje = fragmentarDatos(mensajeListaUsuarios, tamanoMaximoCargaUtil);
            int numeroSecuencia = 1;
            int totalRebanadas = rebanadasMensaje.size();
            for (int i = 0; i < totalRebanadas; i++) {
                std::memset(bufferSalida, '#', 500);
                sprintf(bufferSalida, "%02d%02d%05dl%s\0", numeroSecuencia++, totalRebanadas, (int)rebanadasMensaje[i].length() + 1, rebanadasMensaje[i].c_str());
                sendto(socketServidor, bufferSalida, 500, 0,
                       (sockaddr *)&destinoCliente, sizeof(destinoCliente));
            }
        } else {
            std::memset(bufferSalida, '#', 500);
            sprintf(bufferSalida, "0001%05dl%s\0", (int)mensajeListaUsuarios.length() + 1, mensajeListaUsuarios.c_str());
            sendto(socketServidor, bufferSalida, 500, 0,
                   (sockaddr *)&destinoCliente, sizeof(destinoCliente));
        }
    }

    else if (tipoPaqueteDatos == 'M') {
        int sobrecargaFija = 20 + aliasCliente.length();
        int tamanoMaximoCargaUtil = 500 - sobrecargaFija;

        int tamanoPrimeraParteMensaje = std::stoi(fragmentosRecibidos[0].substr(10, 5));
        int tamanoNombreDestinatario = std::stoi(fragmentosRecibidos[0].substr(15 + tamanoPrimeraParteMensaje, 5));
        std::string nombreDestinatario = fragmentosRecibidos[0].substr(15 + tamanoPrimeraParteMensaje + 5, tamanoNombreDestinatario);

        std::cout << "Destinatario: " << nombreDestinatario.c_str() << "\n";

        std::string contenidoMensajeCompleto;
        for (size_t i = 0; i < fragmentosRecibidos.size(); ++i) {
            int tamanoMensajeActual = std::stoi(fragmentosRecibidos[i].substr(10, 5));
            contenidoMensajeCompleto += fragmentosRecibidos[i].substr(15, tamanoMensajeActual);
        }
        std::cout << "Mensaje: " << contenidoMensajeCompleto.c_str() << "\n";

        sockaddr_in direccionClienteObjetivo = clientesConectados[nombreDestinatario];

        std::vector<std::string> rebanadasMensaje = (contenidoMensajeCompleto.length() > tamanoMaximoCargaUtil)
                                          ? fragmentarDatos(contenidoMensajeCompleto, tamanoMaximoCargaUtil)
                                          : std::vector<std::string>{contenidoMensajeCompleto};

        int totalRebanadas = rebanadasMensaje.size();
        int numeroSecuencia = 1;

        for (auto &rebanada : rebanadasMensaje) {
            std::memset(bufferSalida, '#', 500);

            std::sprintf(bufferSalida, "%02d%02d", (numeroSecuencia == totalRebanadas ? 0 : numeroSecuencia), totalRebanadas);
            int tamanoLogicoTotal = 1 + 5 + rebanada.length() + 5 + aliasCliente.length();

            int desplazamiento = 4;

            std::sprintf(bufferSalida + desplazamiento, "%05d", tamanoLogicoTotal);
            desplazamiento += 5;
            bufferSalida[desplazamiento++] = 'm';

            std::sprintf(bufferSalida + desplazamiento, "%05d", (int)rebanada.length());
            desplazamiento += 5;
            std::memcpy(bufferSalida + desplazamiento, rebanada.c_str(), rebanada.length());
            desplazamiento += rebanada.length();

            std::sprintf(bufferSalida + desplazamiento, "%05d", (int)aliasCliente.length());
            desplazamiento += 5;
            std::memcpy(bufferSalida + desplazamiento, aliasCliente.c_str(), aliasCliente.length());

            sendto(socketServidor, bufferSalida, 500, 0, (sockaddr *)&direccionClienteObjetivo, sizeof(direccionClienteObjetivo));
            ++numeroSecuencia;
        }
    }

    else if (tipoPaqueteDatos == 'B') {
        int sobrecargaFija = 20 + aliasCliente.length();
        int tamanoMaximoCargaUtil = 500 - sobrecargaFija;

        std::string contenidoMensajeCompleto;
        for (size_t i = 0; i < fragmentosRecibidos.size(); ++i) {
            int tamanoMensajeActual = std::stoi(fragmentosRecibidos[i].substr(10, 5));
            contenidoMensajeCompleto += fragmentosRecibidos[i].substr(15, tamanoMensajeActual);
        }

        std::vector<std::string> rebanadasMensaje = (contenidoMensajeCompleto.length() > tamanoMaximoCargaUtil)
                                          ? fragmentarDatos(contenidoMensajeCompleto, tamanoMaximoCargaUtil)
                                          : std::vector<std::string>{contenidoMensajeCompleto};

        int totalRebanadas = rebanadasMensaje.size();
        int numeroSecuencia = 1;

        for (auto &rebanada : rebanadasMensaje) {
            std::memset(bufferSalida, '#', 500);

            std::sprintf(bufferSalida, "%02d%02d", (numeroSecuencia == totalRebanadas ? 0 : numeroSecuencia), totalRebanadas);
            int tamanoLogicoTotal = 1 + 5 + rebanada.length() + 5 + aliasCliente.length();

            int desplazamiento = 4;

            std::sprintf(bufferSalida + desplazamiento, "%05d", tamanoLogicoTotal);
            desplazamiento += 5;
            bufferSalida[desplazamiento++] = 'b';

            std::sprintf(bufferSalida + desplazamiento, "%05d", (int)rebanada.length());
            desplazamiento += 5;
            std::memcpy(bufferSalida + desplazamiento, rebanada.c_str(), rebanada.length());
            desplazamiento += rebanada.length();

            std::sprintf(bufferSalida + desplazamiento, "%05d", (int)aliasCliente.length());
            desplazamiento += 5;
            std::memcpy(bufferSalida + desplazamiento, aliasCliente.c_str(), aliasCliente.length());

            for (const auto &entradaCliente : clientesConectados) {
                if (entradaCliente.first == aliasCliente)
                    continue;

                const sockaddr_in &direccionObjetivo = entradaCliente.second;
                sendto(socketServidor, bufferSalida, 500, 0, (const sockaddr *)&direccionObjetivo, sizeof(direccionObjetivo));
            }
            ++numeroSecuencia;
        }
    }

    else if (tipoPaqueteDatos == 'F') {
        int tamanoDestinatario = std::stoi(fragmentosRecibidos[0].substr(10, 5));
        std::string destinatarioArchivo = fragmentosRecibidos[0].substr(15, tamanoDestinatario);
        int tamanoNombreArchivo = std::stoi(fragmentosRecibidos[0].substr(15 + tamanoDestinatario, 100));
        std::string nombreArchivo = fragmentosRecibidos[0].substr(15 + tamanoDestinatario + 100, tamanoNombreArchivo);
        long tamanoDatosArchivo = std::stol(fragmentosRecibidos[0].substr(15 + tamanoDestinatario + 100 + tamanoNombreArchivo, 18));
        std::string hashArchivo = fragmentosRecibidos[0].substr(15 + tamanoDestinatario + 100 + tamanoNombreArchivo + 18 + tamanoDatosArchivo, 5);

        int sobrecargaFija = 138 + aliasCliente.length() + nombreArchivo.length();
        int tamanoMaximoCargaUtil = 500 - sobrecargaFija;

        std::string contenidoArchivoCompleto;
        for (size_t i = 0; i < fragmentosRecibidos.size(); ++i) {
            long longitudTrozoActual = std::stol(fragmentosRecibidos[i].substr(15 + tamanoDestinatario + 100 + tamanoNombreArchivo, 18));
            size_t posicionInicioDatos = 15 + tamanoDestinatario + 100 + tamanoNombreArchivo + 18;
            contenidoArchivoCompleto += fragmentosRecibidos[i].substr(posicionInicioDatos, longitudTrozoActual);
        }

        std::vector<std::string> rebanadasContenidoArchivo = (contenidoArchivoCompleto.length() > tamanoMaximoCargaUtil)
                                          ? fragmentarDatos(contenidoArchivoCompleto, tamanoMaximoCargaUtil)
                                          : std::vector<std::string>{contenidoArchivoCompleto};

        int totalRebanadas = rebanadasContenidoArchivo.size();
        int numeroSecuencia = 1;

        for (auto &rebanada : rebanadasContenidoArchivo) {
            std::memset(bufferSalida, '#', 500);

            std::sprintf(bufferSalida, "%02d%02d", (numeroSecuencia == totalRebanadas ? 0 : numeroSecuencia), totalRebanadas);

            int tamanoLogicoTotal = 1
                                   + 5 + aliasCliente.length()
                                   + 100 + nombreArchivo.length()
                                   + 18 + rebanada.length()
                                   + 5;

            int desplazamiento = 4;

            std::sprintf(bufferSalida + desplazamiento, "%05d", tamanoLogicoTotal);
            desplazamiento += 5;
            bufferSalida[desplazamiento++] = 'f';

            std::sprintf(bufferSalida + desplazamiento, "%05d", (int)aliasCliente.length());
            desplazamiento += 5;
            std::memcpy(bufferSalida + desplazamiento, aliasCliente.c_str(), aliasCliente.length());
            desplazamiento += aliasCliente.length();

            std::sprintf(bufferSalida + desplazamiento, "%0100d", (int)nombreArchivo.length());
            desplazamiento += 100;
            std::memcpy(bufferSalida + desplazamiento, nombreArchivo.c_str(), nombreArchivo.length());
            desplazamiento += nombreArchivo.length();

            std::sprintf(bufferSalida + desplazamiento, "%018d", (int)rebanada.length());
            desplazamiento += 18;
            std::memcpy(bufferSalida + desplazamiento, rebanada.c_str(), rebanada.length());
            desplazamiento += rebanada.length();

            std::memcpy(bufferSalida + desplazamiento, hashArchivo.c_str(), 5);

            sockaddr_in direccionClienteObjetivo = clientesConectados[destinatarioArchivo];
            sendto(socketServidor, bufferSalida, 500, 0,
                   (sockaddr *)&direccionClienteObjetivo, sizeof(direccionClienteObjetivo));
            ++numeroSecuencia;
        }
    }

    else if (tipoPaqueteDatos == 'Q') {
        printf("\nEl cliente %s ha salido del chat\n", aliasCliente.c_str());
        clientesConectados.erase(aliasCliente);
        return;
    }

    else if (tipoPaqueteDatos == 'J') {
        if (!juegoActualTicTacToe.estaActiva && jugadorEnEspera.empty()) {
            jugadorEnEspera = aliasCliente;
            sockaddr_in direccionCliente = clientesConectados[aliasCliente];
            enviarMensajePrivado(socketServidor, direccionCliente, "esperando a un jugador para empezar");
        }

        else if (!juegoActualTicTacToe.estaActiva && !jugadorEnEspera.empty()) {
            juegoActualTicTacToe.estaActiva = true;
            juegoActualTicTacToe.jugadorCirculo = jugadorEnEspera;
            juegoActualTicTacToe.jugadorCruz = aliasCliente;
            jugadorEnEspera = "";

            enviarMensajePrivado(socketServidor, clientesConectados[juegoActualTicTacToe.jugadorCirculo], "inicio");
            enviarMensajePrivado(socketServidor, clientesConectados[juegoActualTicTacToe.jugadorCruz], "inicio");

            enviarTableroATodos(socketServidor);
            notificarTurno(socketServidor, juegoActualTicTacToe.jugadorCirculo, 'O');
        } else {
            enviarMensajePrivado(socketServidor, clientesConectados[aliasCliente], "quieres ver la partida?");
        }
    }

    else if (tipoPaqueteDatos == 'V') {
        if (juegoActualTicTacToe.estaActiva)
            juegoActualTicTacToe.observadoresJuego.push_back(aliasCliente);

        char paqueteJuego[500];
        std::memset(paqueteJuego, '#', 500);

        std::memcpy(paqueteJuego, "0001", 4);

        std::memcpy(paqueteJuego + 4, "00010", 5);

        paqueteJuego[9] = 'X';
        std::memcpy(paqueteJuego + 10, juegoActualTicTacToe.tableroJuego, 9);

        sockaddr_in direccionCliente = clientesConectados[aliasCliente];
        sendto(socketServidor, paqueteJuego, 500, 0,
               (sockaddr *)&direccionCliente, sizeof(direccionCliente));
    }

    else if (tipoPaqueteDatos == 'P') {
        char posicionMovimiento = fragmentosRecibidos[0][10];
        char simboloJugador = fragmentosRecibidos[0][11];

        int indiceTablero = posicionMovimiento - '1';
        if (indiceTablero < 0 || indiceTablero > 8 || juegoActualTicTacToe.tableroJuego[indiceTablero] != '_') {
            char paqueteError[500];
            std::memset(paqueteError, '#', 500);

            std::memcpy(paqueteError, "0001", 4);

            std::memcpy(paqueteError + 4, "00018", 5);

            int desplazamiento = 9;
            paqueteError[desplazamiento++] = 'E';
            paqueteError[desplazamiento++] = '6';
            std::memcpy(paqueteError + desplazamiento, "00016", 5);
            desplazamiento += 5;
            std::memcpy(paqueteError + desplazamiento, "Posicion ocupada", 16);
            const sockaddr_in &direccionCliente = clientesConectados[aliasCliente];

            sendto(socketServidor, paqueteError, 500, 0,
                   (const sockaddr *)&direccionCliente, sizeof(direccionCliente));
            return;
        }

        juegoActualTicTacToe.tableroJuego[indiceTablero] = simboloJugador;

        if (verificarCondicionVictoria(simboloJugador)) {
            enviarTableroATodos(socketServidor);
            sleep(0.5);
        
            auto enviarResultadoJuego = [&](const std::string &alias, char resultado) {
                char paqueteResultado[500];
                std::memset(paqueteResultado, '#', 500);
                std::memcpy(paqueteResultado, "0001", 4);
                std::memcpy(paqueteResultado + 4, "00002", 5);
                paqueteResultado[9] = 'O';
                paqueteResultado[10] = resultado;
                sendto(socketServidor, paqueteResultado, 500, 0, (sockaddr *)&clientesConectados[alias], sizeof(sockaddr_in));
            };

            enviarResultadoJuego(juegoActualTicTacToe.jugadorCirculo, (simboloJugador == 'O') ? 'W' : 'L');
            enviarResultadoJuego(juegoActualTicTacToe.jugadorCruz, (simboloJugador == 'X') ? 'W' : 'L');
            for (auto &observador : juegoActualTicTacToe.observadoresJuego)
                enviarResultadoJuego(observador, 'E');

            juegoActualTicTacToe = SesionJuego();
        } else if (estaTableroCompleto()) {
            enviarTableroATodos(socketServidor);

            char paqueteEmpate[500];
            std::memset(paqueteEmpate, '#', 500);
            std::memcpy(paqueteEmpate, "0001", 4);
            std::memcpy(paqueteEmpate + 4, "00002", 5);
            paqueteEmpate[9] = 'O';
            paqueteEmpate[10] = 'E';

            for (auto alias : {juegoActualTicTacToe.jugadorCirculo, juegoActualTicTacToe.jugadorCruz})
                sendto(socketServidor, paqueteEmpate, 500, 0, (sockaddr *)&clientesConectados[alias], sizeof(sockaddr_in));
            for (auto &observador : juegoActualTicTacToe.observadoresJuego)
                sendto(socketServidor, paqueteEmpate, 500, 0, (sockaddr *)&clientesConectados[observador], sizeof(sockaddr_in));

            juegoActualTicTacToe = SesionJuego();
        } else {
            enviarTableroATodos(socketServidor);
            juegoActualTicTacToe.simboloJugadorActual = (juegoActualTicTacToe.simboloJugadorActual == 'O') ? 'X' : 'O';
            const std::string &siguienteJugador =
                (juegoActualTicTacToe.simboloJugadorActual == 'O') ? juegoActualTicTacToe.jugadorCirculo
                                                                   : juegoActualTicTacToe.jugadorCruz;
            notificarTurno(socketServidor, siguienteJugador, juegoActualTicTacToe.simboloJugadorActual);
        }
    }
}

void enviarMensajePrivado(int idSocket,
                        const sockaddr_in &direccionDestino, 
                        const std::string &textoMensaje)
{
  const std::string identificadorRemitente = "servidor";
  const int longitudMensaje = (int)textoMensaje.size();
  const int longitudRemitente = (int)identificadorRemitente.size();

  int tamanoLogicoTotal = 1 + 5 + longitudMensaje + 5 + longitudRemitente;
  char bufferPaquete[500];
  std::memset(bufferPaquete, '#', 500);

  std::memcpy(bufferPaquete, "0001", 4);

  std::sprintf(bufferPaquete + 4, "%05d", tamanoLogicoTotal);
  int desplazamiento = 9;
  bufferPaquete[desplazamiento++] = 'm';

  std::sprintf(bufferPaquete + desplazamiento, "%05d", longitudMensaje);
  desplazamiento += 5;
  std::memcpy(bufferPaquete + desplazamiento, textoMensaje.c_str(), longitudMensaje);
  desplazamiento += longitudMensaje;

  std::sprintf(bufferPaquete + desplazamiento, "%05d", longitudRemitente);
  desplazamiento += 5;
  std::memcpy(bufferPaquete + desplazamiento, identificadorRemitente.c_str(), longitudRemitente);
  if (sendto(idSocket, bufferPaquete, 500, 0,
             (const sockaddr *)&direccionDestino, sizeof(direccionDestino)) == -1)
    perror("sendto");
}

void enviarTableroATodos(int idSocket) {
  char paquete[500];
  std::memset(paquete, '#', 500);

  std::memcpy(paquete, "0001", 4);

  std::memcpy(paquete + 4, "00010", 5);

  paquete[9] = 'X';
  std::memcpy(paquete + 10, juegoActualTicTacToe.tableroJuego, 9);
  for (int i = 0; paquete[i] != '#' && paquete[i] != '\0'; ++i)
  {
    std::cout << paquete[i];
  }
  std::cout << std::endl;

  for (auto alias : {juegoActualTicTacToe.jugadorCirculo, juegoActualTicTacToe.jugadorCruz})
    if (!alias.empty())
      sendto(idSocket, paquete, 500, 0, (sockaddr *)&clientesConectados[alias], sizeof(sockaddr_in));

  for (auto &observador : juegoActualTicTacToe.observadoresJuego)
    sendto(idSocket, paquete, 500, 0, (sockaddr *)&clientesConectados[observador], sizeof(sockaddr_in));
}

void notificarTurno(int idSocket, const std::string &alias, char simbolo) {
  char paquete[500];
  std::memset(paquete, '#', 500);
  std::memcpy(paquete, "0001", 4);

  std::memcpy(paquete + 4, "00002", 5);

  paquete[9] = 'T';
  paquete[10] = simbolo;

  for (int i = 0; paquete[i] != '#' && paquete[i] != '\0'; ++i)
  {
    std::cout << paquete[i];
  }
  std::cout << std::endl;

  sendto(idSocket, paquete, 500, 0, (sockaddr *)&clientesConectados[alias], sizeof(sockaddr_in));
}

bool verificarLinea(int a, int b, int c, char s) {
  return juegoActualTicTacToe.tableroJuego[a] == s && juegoActualTicTacToe.tableroJuego[b] == s && juegoActualTicTacToe.tableroJuego[c] == s;
}
bool verificarCondicionVictoria(char simbolo) {
  return verificarLinea(0, 1, 2, simbolo) || verificarLinea(3, 4, 5, simbolo) || verificarLinea(6, 7, 8, simbolo) ||
         verificarLinea(0, 3, 6, simbolo) || verificarLinea(1, 4, 7, simbolo) || verificarLinea(2, 5, 8, simbolo) ||
         verificarLinea(0, 4, 8, simbolo) || verificarLinea(2, 4, 6, simbolo);
}
bool estaTableroCompleto() {
  for (char c : juegoActualTicTacToe.tableroJuego)
    if (c == '_')
      return false;
  return true;
}

int main(void) {
  char bufferRecibido[500];

  int socketPrincipal;
  int longitudDir, bytesLeidos;
  struct sockaddr_in direccionServidor, direccionCliente;

  if ((socketPrincipal = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("Socket");
    exit(1);
  }

  direccionServidor.sin_family = AF_INET;
  direccionServidor.sin_port = htons(5000);
  direccionServidor.sin_addr.s_addr = INADDR_ANY;
  bzero(&(direccionServidor.sin_zero), 8);

  if (bind(socketPrincipal, (struct sockaddr *)&direccionServidor,
           sizeof(struct sockaddr)) == -1)
  {
    perror("Bind");
    exit(1);
  }

  longitudDir = sizeof(struct sockaddr);
  sockaddr_in direccionClienteActual;
  socklen_t longitudCliente = sizeof(direccionClienteActual);

  for (;;)
  {
    int bytesRecibidos = recvfrom(socketPrincipal, bufferRecibido, 500, 0,
                     (sockaddr *)&direccionClienteActual, &longitudCliente);
    if (bytesRecibidos != 500)
    {
      std::cout << "ERROR size mensaje\n";
      continue;
    }

    char claveCliente[32];
    sprintf(claveCliente, "%s:%d",
            inet_ntoa(direccionClienteActual.sin_addr), ntohs(direccionClienteActual.sin_port));

    std::string paquete(bufferRecibido, 500);

    if (paquete[9] == 'N')
    {
      std::cout << paquete.c_str() << "\n";
      int tamanoAlias = std::stoi(paquete.substr(4, 5)) - 1;
      std::string alias = paquete.substr(10, tamanoAlias);
      clientesConectados[alias] = direccionClienteActual;
      std::cout << "El cliente " << alias.c_str() << " se ha unido al chat" << "\n";
      continue;
    }

    std::string alias;
    for (auto &parClaveValor : clientesConectados)
      if (memcmp(&parClaveValor.second, &direccionClienteActual, sizeof(direccionClienteActual)) == 0)
      {
        alias = parClaveValor.first;
        break;
      }
    if (alias.empty())
      continue;

    int numeroSecuencia = std::stoi(std::string(paquete, 0, 2));
    int totalPaquetes = std::stoi(std::string(paquete, 2, 2));
    int indice = (numeroSecuencia == 0) ? totalPaquetes - 1 : numeroSecuencia - 1;

    auto &vectorPaquetes = buffersPaquetesCliente[claveCliente];
    if (vectorPaquetes.empty())
      vectorPaquetes.resize(totalPaquetes);

    if (indice >= totalPaquetes)
      continue;
    if (!vectorPaquetes[indice].empty())
      continue;

    vectorPaquetes[indice] = std::move(paquete);

    bool estaCompleto = true;
    for (auto &p : vectorPaquetes)
      if (p.empty())
      {
        estaCompleto = false;
        break;
      }

    if (estaCompleto)
    {
      char tipo = vectorPaquetes[0][9];
      std::vector<std::string> partes = std::move(vectorPaquetes);
      buffersPaquetesCliente.erase(claveCliente);

      std::thread(procesarDatosCliente, std::move(partes),
                  alias, socketPrincipal, tipo)
          .detach();
    }
  }
  close(socketPrincipal);
  return 0;
}

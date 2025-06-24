#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <limits>
#include <fstream>
#include <vector>
#include <mutex>
#include <cstring>

std::string identificadorUsuario;
std::atomic<bool> listaObtenida(false);
std::atomic<bool> enPartidaActiva(false);
std::atomic<bool> esObservador(false);
std::atomic<char> marcaJuego{'\0'};
std::atomic<bool> esMiTurno(false);
std::atomic<bool> consultaVisualizacion(false);
std::mutex exclusividadConsola;
int descriptorSocketCliente;

void manejarMensajesEntrantes(std::vector<std::string> fragmentosMensaje, std::string aliasActual, int descriptorSock, char tipoMensaje);

std::vector<std::string> dividirMensaje(std::string contenidoMensaje, int tamanoMaxFragmento) {
    std::vector<std::string> fragmentos;
    int longitudMensaje = contenidoMensaje.size();

    for (int i = 0; i < longitudMensaje; i += tamanoMaxFragmento) {
        fragmentos.push_back(contenidoMensaje.substr(i, tamanoMaxFragmento));
    }
    return fragmentos;
}

int calcularSumaVerificacion(const char *bufferDatos, long tamanoDatos) {
    int valorSumaVerificacion = 0;
    for (long i = 0; i < tamanoDatos; i++) {
        valorSumaVerificacion = (valorSumaVerificacion + (unsigned char)bufferDatos[i]) % 100000;
    }
    return valorSumaVerificacion;
}

void recibirDatosServidor(int descriptorSock, sockaddr_in puntoExtremoServidor) {
    char bufferDatos[500];

    static std::vector<std::string> fragmentosActuales;
    fragmentosActuales.clear();

    struct sockaddr_in direccionRemitente;
    socklen_t longitudDireccion = sizeof(direccionRemitente);

    while (true) {
        int bytesRecibidos = recvfrom(descriptorSock, bufferDatos, 500, 0,
                                     (struct sockaddr *)&direccionRemitente, &longitudDireccion);
        if (bytesRecibidos != 500) {
            std::cout << "ERROR: Desajuste en el tamaño del mensaje\n";
            continue;
        }

        std::string paqueteRecibido(bufferDatos, 500);

        int numeroSecuencia = std::stoi(paqueteRecibido.substr(0, 2));
        int totalFragmentos = std::stoi(paqueteRecibido.substr(2, 2));
        int indiceFragmento = (numeroSecuencia == 0) ? totalFragmentos - 1
                                                   : numeroSecuencia - 1;
        if (indiceFragmento < 0 || indiceFragmento >= totalFragmentos)
            continue;

        if (fragmentosActuales.empty())
            fragmentosActuales.resize(totalFragmentos);
        if (indiceFragmento >= (int)fragmentosActuales.size())
            continue;
        if (!fragmentosActuales[indiceFragmento].empty())
            continue;
        fragmentosActuales[indiceFragmento] = std::move(paqueteRecibido);

        bool estaCompleto = true;
        for (auto &frag : fragmentosActuales)
            if (frag.empty()) {
                estaCompleto = false;
                break;
            }

        if (!estaCompleto)
            continue;

        char tipoMsg = fragmentosActuales[0][9];

        std::vector<std::string> partesMensaje = std::move(fragmentosActuales);
        fragmentosActuales.clear();

        std::thread(manejarMensajesEntrantes,
                    std::move(partesMensaje),
                    identificadorUsuario,
                    descriptorSock,
                    tipoMsg)
            .detach();
    }
}

void manejarMensajesEntrantes(std::vector<std::string> fragmentosMensaje, std::string aliasActual, int descriptorSock, char tipoMensaje) {
    char bufferTemporal[500];

    if (tipoMensaje == 'l') {
        std::string mensajeReconstruido;
        for (size_t i = 0; i < fragmentosMensaje.size(); ++i) {
            int longitudParte = std::stoi(fragmentosMensaje[i].substr(4, 5)) - 1;
            mensajeReconstruido += fragmentosMensaje[i].substr(10, longitudParte);
        }
        printf("\nUsuarios conectados: %s\n", mensajeReconstruido.c_str());
        listaObtenida = true;
        return;
    }
    else if (tipoMensaje == 'm') {
        int tamanoPrimerMensaje = std::stoi(fragmentosMensaje[0].substr(10, 5));
        int tamanoUsuarioOrigen = std::stoi(fragmentosMensaje[0].substr(15 + tamanoPrimerMensaje, 5));
        std::string usuarioRemitente = fragmentosMensaje[0].substr(15 + tamanoPrimerMensaje + 5, tamanoUsuarioOrigen);

        std::string contenidoMensaje;
        for (size_t i = 0; i < fragmentosMensaje.size(); ++i) {
            int tamanoMensajeActual = std::stoi(fragmentosMensaje[i].substr(10, 5));
            contenidoMensaje += fragmentosMensaje[i].substr(15, tamanoMensajeActual);
        }

        printf("\n\n%s dice : %s\n", usuarioRemitente.c_str(), contenidoMensaje.c_str());

        if ((usuarioRemitente == "servidor" || usuarioRemitente == "Servidor") &&
        (contenidoMensaje == "quieres ver la partida?")){
            {
                std::lock_guard<std::mutex> bloqueoConsola(exclusividadConsola);
                std::cout << "y. Sí   n. No\n";
                std::cout.flush();
            }
            consultaVisualizacion = true;
            return;
        }
    }
    else if (tipoMensaje == 'b') {
        int tamanoPrimerMensaje = std::stoi(fragmentosMensaje[0].substr(10, 5));
        int tamanoUsuarioOrigen = std::stoi(fragmentosMensaje[0].substr(15 + tamanoPrimerMensaje, 5));
        std::string usuarioRemitente = fragmentosMensaje[0].substr(15 + tamanoPrimerMensaje + 5, tamanoUsuarioOrigen);

        std::string contenidoMensaje;
        for (size_t i = 0; i < fragmentosMensaje.size(); ++i) {
            int tamanoMensajeActual = std::stoi(fragmentosMensaje[i].substr(10, 5));
            contenidoMensaje += fragmentosMensaje[i].substr(15, tamanoMensajeActual);
        }

        printf("\n\n[Mensaje para todos] %s: %s\n", usuarioRemitente.c_str(), contenidoMensaje.c_str());
    }
    else if (tipoMensaje == 'f') {
        int tamanoRemitente = std::stoi(fragmentosMensaje[0].substr(10, 5));
        std::string nombreRemitente = fragmentosMensaje[0].substr(15, tamanoRemitente);
        int tamanoNombreArchivo = std::stoi(fragmentosMensaje[0].substr(15 + tamanoRemitente, 100));
        std::string nombreArchivoRecibido = fragmentosMensaje[0].substr(15 + tamanoRemitente + 100, tamanoNombreArchivo);
        long valorTamanoArchivo = std::stol(fragmentosMensaje[0].substr(15 + tamanoRemitente + 100 + tamanoNombreArchivo, 18));
        std::string hashRecibido = fragmentosMensaje[0].substr(15 + tamanoRemitente + 100 + tamanoNombreArchivo + 18 + valorTamanoArchivo, 5);

        std::string contenidoArchivo;
        for (size_t i = 0; i < fragmentosMensaje.size(); ++i) {
            long longitudTrozo = std::stol(fragmentosMensaje[i].substr(15 + tamanoRemitente + 100 + tamanoNombreArchivo, 18));
            size_t posicionDatos = 15 + tamanoRemitente + 100 + tamanoNombreArchivo + 18;
            contenidoArchivo += fragmentosMensaje[i].substr(posicionDatos, longitudTrozo);
        }

        int longitudFinalArchivo = contenidoArchivo.length();

        int hashLocal = calcularSumaVerificacion(contenidoArchivo.c_str(), longitudFinalArchivo);

        if (std::stoi(hashRecibido) == hashLocal)
            printf("\n[archivo] %s: %s (Hash OK)\n", nombreRemitente.c_str(), nombreArchivoRecibido.c_str());
        else
            printf("\n[archivo] %s: %s (Hash INCORRECTO: calculado %d, recibido %s)\n",
                   nombreRemitente.c_str(), nombreArchivoRecibido.c_str(), hashLocal, hashRecibido.c_str());

        if (std::stoi(hashRecibido) == hashLocal) {
            std::ofstream archivoSalida("nuevo"+nombreArchivoRecibido, std::ios::binary);
            if (!archivoSalida.is_open()) {
                std::cerr << "No se pudo crear el archivo " << nombreArchivoRecibido << " en disco.\n";
            } else {
                archivoSalida.write(contenidoArchivo.c_str(), longitudFinalArchivo);
                archivoSalida.close();
                std::cout << "Archivo " << nombreArchivoRecibido << " guardado en la carpeta actual.\n";
            }
        }
    }

    else if (tipoMensaje == 'x' || tipoMensaje == 'X') {
        puts("\nTABLERO DE TIC-TAC-TOE");

        for (int j = 0; j < 9; ++j) {
            char caracterJuego = fragmentosMensaje[0][10 + j];
            std::putchar(caracterJuego);
            std::putchar((j % 3 == 2) ? '\n' : '|');
        }
    }

    else if (tipoMensaje == 't' || tipoMensaje == 'T') {
        marcaJuego = fragmentosMensaje[0][10];
        esMiTurno = true;

        {
            std::lock_guard<std::mutex> bloqueoConsola(exclusividadConsola);
            std::cout << "\n───────────────────────────────\n"
                         "¡ES TU TURNO! ("
                      << marcaJuego << ")\n Ingresa una posición [1-9]: \n";
            std::cout.flush();
        }
    }

    else if (tipoMensaje == 'e' || tipoMensaje == 'E') {
        char codigoError;
        codigoError = fragmentosMensaje[0][10];

        int tamanoContenidoMensaje = std::stoi(fragmentosMensaje[0].substr(11, 5));
        std::string mensajeError = fragmentosMensaje[0].substr(11 + 5, tamanoContenidoMensaje);

        std::cout << "\nERROR " << codigoError << ": " << bufferTemporal << "\n";

        if (codigoError == '6') {
            esMiTurno = true;
        }
    }

    else if (tipoMensaje == 'o' || tipoMensaje == 'O') {
        char resultadoJuego = fragmentosMensaje[0][10];
        if (!esObservador) {
            if (resultadoJuego == 'W')
                puts("\n ¡Ganaste!");
            else if (resultadoJuego == 'L')
                puts("\n ¡Perdiste! ");
            else
                puts("\n ¡Empate! ");
        } else {
            puts("\n*** La partida ha terminado ***");
        }
        enPartidaActiva = false;
        esObservador = false;
    }
}

void enviarMovimientoJuego(int descriptorSock, const sockaddr_in &puntoExtremoServidor, int posicion, char simbolo) {
    char bufferPaquete[500];
    std::memset(bufferPaquete, '#', 500);
    int tamanoLogicoTotal = 3;

    std::memcpy(bufferPaquete, "0001", 4);

    std::sprintf(bufferPaquete + 4, "%05d", tamanoLogicoTotal);
    int desplazamiento = 9;
    char caracterPosicion = posicion + '0';
    bufferPaquete[desplazamiento++] = 'P';
    bufferPaquete[desplazamiento++] = caracterPosicion;
    bufferPaquete[desplazamiento++] = simbolo;

    if (sendto(descriptorSock, bufferPaquete, 500, 0,
               (const sockaddr *)&puntoExtremoServidor, sizeof(puntoExtremoServidor)) == -1)
        perror("sendto");
}

int main(void) {
    int socketUDP;
    struct sockaddr_in direccionServidor;
    struct hostent *infoHost;
    char paqueteDatos[500];

    infoHost = (struct hostent *)gethostbyname((char *)"127.0.0.1");

    if ((socketUDP = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(5000);
    direccionServidor.sin_addr = *((struct in_addr *)infoHost->h_addr);
    bzero(&(direccionServidor.sin_zero), 8);

    descriptorSocketCliente = socketUDP;

    std::cout << "Ingresa tu alias: ";
    std::getline(std::cin, identificadorUsuario);
    std::memset(paqueteDatos, '#', 500);
    sprintf(paqueteDatos, "0001%05dN%s\0", (int)identificadorUsuario.length() + 1, identificadorUsuario.c_str());
    sendto(socketUDP, paqueteDatos, 500, 0,
           (struct sockaddr *)&direccionServidor, sizeof(struct sockaddr));
    std::thread(recibirDatosServidor, socketUDP, direccionServidor).detach();

    bool salirCliente = false;

    while (!salirCliente) {
        {
            std::lock_guard<std::mutex> bloqueoConsola(exclusividadConsola);
            if (!enPartidaActiva) {
                std::cout << "\n=== MENÚ PRINCIPAL ===\n"
                             "a) Ver lista de usuarios\n"
                             "b) Enviar mensaje privado\n"
                             "c) Salir\n"
                             "d) Enviar mensaje de difusión\n"
                             "e) Enviar archivo\n"
                             "f) Jugar 3 en raya\n";
            }
            if (esMiTurno)
                std::cout << "(Es tu turno, teclea 1-9 para marcar una casilla)\n";
            if (consultaVisualizacion) {
                std::cout << "do you want to see?\n";
            }
            std::cout << "> ";
            std::cout.flush();
        }

        char eleccionUsuario;
        std::cin >> eleccionUsuario;

        if (esMiTurno && eleccionUsuario >= '1' && eleccionUsuario <= '9') {
            enviarMovimientoJuego(socketUDP, direccionServidor, eleccionUsuario - '0', marcaJuego);
            esMiTurno = false;
            continue;
        }

        if (consultaVisualizacion) {
            if (eleccionUsuario == 'y' || eleccionUsuario == 'Y') {
                esObservador = true;
                std::memset(paqueteDatos, '#', 500);
                std::memcpy(paqueteDatos, "0001", 4);
                std::sprintf(paqueteDatos + 4, "%05d", 1);
                int desplazamiento = 9;
                paqueteDatos[desplazamiento++] = 'V';

                if (sendto(socketUDP, paqueteDatos, 500, 0,
                           (const sockaddr *)&direccionServidor, sizeof(direccionServidor)) == -1)
                    perror("sendto");
            } else if (eleccionUsuario == 'n' || eleccionUsuario == 'N') {
                enPartidaActiva = false;
            } else {
                std::cout << "Comando no válido.\n";
                continue;
            }
            consultaVisualizacion = false;
        }

        else if (eleccionUsuario == 'a') {
            std::memset(paqueteDatos, '#', 500);
            strcpy(paqueteDatos, "000100001L");
            if (sendto(socketUDP, paqueteDatos, 500, 0,
                       (const sockaddr *)&direccionServidor, sizeof(direccionServidor)) == -1)
                perror("sendto");
            while (!listaObtenida)
                ;
            listaObtenida = false;
        }
        else if (eleccionUsuario == 'b') {
            std::string nombreDestinatario;
            std::string mensajeAEnviar;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\nNombre Destinatario: ";
            std::getline(std::cin, nombreDestinatario);
            std::cout << "\nMensaje: ";
            std::getline(std::cin, mensajeAEnviar);

            int longitudMensaje = mensajeAEnviar.length();
            int longitudDestinatario = nombreDestinatario.length();
            int tamanoCabeceraFija = 20 + longitudDestinatario;
            int tamanoMaxContenido = 500 - tamanoCabeceraFija;

            std::vector<std::string> trozosMensaje = (mensajeAEnviar.length() > tamanoMaxContenido)
                                            ? dividirMensaje(mensajeAEnviar, tamanoMaxContenido)
                                            : std::vector<std::string>{mensajeAEnviar};

            int paquetesTotales = trozosMensaje.size();
            int contadorSecuencia = 1;

            for (auto &trozo : trozosMensaje) {
                std::memset(paqueteDatos, '#', 500);

                std::sprintf(paqueteDatos, "%02d%02d", (contadorSecuencia == paquetesTotales ? 0 : contadorSecuencia), paquetesTotales);
                int tamanoTotal = 1 + 5 + trozo.length() + 5 + nombreDestinatario.length();

                int desplazamiento = 4;

                std::sprintf(paqueteDatos + desplazamiento, "%05d", tamanoTotal);
                desplazamiento += 5;
                paqueteDatos[desplazamiento++] = 'M';

                std::sprintf(paqueteDatos + desplazamiento, "%05d", (int)trozo.length());
                desplazamiento += 5;
                std::memcpy(paqueteDatos + desplazamiento, trozo.c_str(), trozo.length());
                desplazamiento += trozo.length();

                std::sprintf(paqueteDatos + desplazamiento, "%05d", (int)nombreDestinatario.length());
                desplazamiento += 5;
                std::memcpy(paqueteDatos + desplazamiento, nombreDestinatario.c_str(), nombreDestinatario.length());

                for (int i = 0; paqueteDatos[i] != '#' && paqueteDatos[i] != '\0'; ++i) {
                    std::cout << paqueteDatos[i];
                }
                std::cout << std::endl;

                sendto(socketUDP, paqueteDatos, 500, 0, (sockaddr *)&direccionServidor, sizeof(direccionServidor));
                ++contadorSecuencia;
            }
        }
        else if (eleccionUsuario == 'c') {
            salirCliente = true;
            std::memset(paqueteDatos, '#', 500);
            strcpy(paqueteDatos, "000100001Q");
            if (sendto(socketUDP, paqueteDatos, 500, 0,
                       (const sockaddr *)&direccionServidor, sizeof(direccionServidor)) == -1)
                perror("sendto");
        }
        else if (eleccionUsuario == 'd') {
            std::string mensajeDifusion;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\nMensaje: ";
            std::getline(std::cin, mensajeDifusion);

            int tamanoCabeceraFija = 15;
            int tamanoMaxContenido = 500 - tamanoCabeceraFija;

            std::vector<std::string> trozosMensaje = (mensajeDifusion.length() > tamanoMaxContenido)
                                            ? dividirMensaje(mensajeDifusion, tamanoMaxContenido)
                                            : std::vector<std::string>{mensajeDifusion};

            int paquetesTotales = trozosMensaje.size();
            int contadorSecuencia = 1;

            for (auto &trozo : trozosMensaje) {
                std::memset(paqueteDatos, '#', 500);

                std::sprintf(paqueteDatos, "%02d%02d", (contadorSecuencia == paquetesTotales ? 0 : contadorSecuencia), paquetesTotales);
                int tamanoTotal = 1 + 5 + trozo.length();

                int desplazamiento = 4;

                std::sprintf(paqueteDatos + desplazamiento, "%05d", tamanoTotal);
                desplazamiento += 5;
                paqueteDatos[desplazamiento++] = 'B';

                std::sprintf(paqueteDatos + desplazamiento, "%05d", (int)trozo.length());
                desplazamiento += 5;
                std::memcpy(paqueteDatos + desplazamiento, trozo.c_str(), trozo.length());
                desplazamiento += trozo.length();

                for (int i = 0; paqueteDatos[i] != '#' && paqueteDatos[i] != '\0'; ++i) {
                    std::cout << paqueteDatos[i];
                }

                std::cout << std::endl;
                sendto(socketUDP, paqueteDatos, 500, 0, (sockaddr *)&direccionServidor, sizeof(direccionServidor));
                ++contadorSecuencia;
            }
        }
        else if (eleccionUsuario == 'e') {
            std::string destinatarioObjetivo;
            std::string rutaArchivo;
            std::string soloNombreArchivo;

            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\nNombre Destinatario: ";
            std::getline(std::cin, destinatarioObjetivo);
            std::cout << "Ruta del archivo a enviar: ";
            std::getline(std::cin, rutaArchivo);

            size_t ultimaPosBarra = rutaArchivo.find_last_of("/\\");
            if (ultimaPosBarra != std::string::npos) {
                soloNombreArchivo = rutaArchivo.substr(ultimaPosBarra + 1);
            } else {
                soloNombreArchivo = rutaArchivo;
            }

            std::ifstream archivoEntrada(rutaArchivo, std::ios::in | std::ios::binary);
            if (!archivoEntrada.is_open()) {
                std::cerr << "No se pudo abrir el archivo " << rutaArchivo << "\n";
                continue;
            }
            std::vector<char> bytesArchivo((std::istreambuf_iterator<char>(archivoEntrada)),
                                        std::istreambuf_iterator<char>());
            long tamanoArchivoTotal = bytesArchivo.size();
            archivoEntrada.close();

            int tamanoCabeceraFija = 138 + destinatarioObjetivo.length() + soloNombreArchivo.length();
            int tamanoMaxTrozo = 500 - tamanoCabeceraFija;

            int hashCalculado = calcularSumaVerificacion(bytesArchivo.data(), tamanoArchivoTotal);
            std::string cadenaHash = std::to_string(hashCalculado);

            std::string datosArchivoCadena(bytesArchivo.begin(), bytesArchivo.end());

            std::vector<std::string> trozosArchivo = (datosArchivoCadena.length() > tamanoMaxTrozo)
                                            ? dividirMensaje(datosArchivoCadena, tamanoMaxTrozo)
                                            : std::vector<std::string>{datosArchivoCadena};

            int paquetesTotales = trozosArchivo.size();

            std::cout << "Total de paquetes a enviar: " << paquetesTotales << "\n";

            int contadorSecuencia = 1;
            for (auto &trozo : trozosArchivo) {
                std::memset(paqueteDatos, '#', 500);

                std::sprintf(paqueteDatos, "%02d%02d", (contadorSecuencia == paquetesTotales ? 0 : contadorSecuencia), paquetesTotales);

                int tamanoTotal = 1
                                + 5 + destinatarioObjetivo.length()
                                + 100 + soloNombreArchivo.length()
                                + 18 + trozo.length()
                                + 5;

                int desplazamiento = 4;

                std::sprintf(paqueteDatos + desplazamiento, "%05d", tamanoTotal);
                desplazamiento += 5;
                paqueteDatos[desplazamiento++] = 'F';

                std::sprintf(paqueteDatos + desplazamiento, "%05d", (int)destinatarioObjetivo.length());
                desplazamiento += 5;
                std::memcpy(paqueteDatos + desplazamiento, destinatarioObjetivo.c_str(), destinatarioObjetivo.length());
                desplazamiento += destinatarioObjetivo.length();

                std::sprintf(paqueteDatos + desplazamiento, "%0100d", (int)soloNombreArchivo.length());
                desplazamiento += 100;
                std::memcpy(paqueteDatos + desplazamiento, soloNombreArchivo.c_str(), soloNombreArchivo.length());
                desplazamiento += soloNombreArchivo.length();

                std::sprintf(paqueteDatos + desplazamiento, "%018d", (int)trozo.length());
                desplazamiento += 18;
                std::memcpy(paqueteDatos + desplazamiento, trozo.c_str(), trozo.length());
                desplazamiento += trozo.length();

                std::memcpy(paqueteDatos + desplazamiento, cadenaHash.c_str(), 5);

                sendto(socketUDP, paqueteDatos, 500, 0,
                       (sockaddr *)&direccionServidor, sizeof(direccionServidor));
                printf("Enviando mensaje %02d%02d\n",contadorSecuencia,paquetesTotales);
                ++contadorSecuencia;
            }
        }
        else if (eleccionUsuario == 'f') {
            if (!enPartidaActiva) {
                std::memset(paqueteDatos, '#', 500);
                strcpy(paqueteDatos, "000100001J");
                if (sendto(socketUDP, paqueteDatos, 500, 0,
                           (const sockaddr *)&direccionServidor, sizeof(direccionServidor)) == -1)
                    perror("sendto");
                enPartidaActiva = true;
            } else {
                std::cout << "Ya estás en una partida.\n";
            }
        }
        else {
            std::cout << "Comando no válido.\n";
        }
    }
    return 0;
}

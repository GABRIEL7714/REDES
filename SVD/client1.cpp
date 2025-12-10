#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>   

using namespace std;

// ===================== Configuración =====================

const char *MASTER_IP      = "127.0.0.1";  
const int   PUERTO_CLIENTE = 47000;        
const int   NUM_WORKERS    = 2;            

// ===================== Helpers de red =====================

ssize_t send_all(int sock, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sock, p + total, len - total, 0);
        if (n <= 0) return n;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t recv_all(int sock, void *buf, size_t len) {
    char *p = (char *)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(sock, p + total, len - total, 0);
        if (n <= 0) return n;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

bool send_byte(int sock, char c) {
    return send_all(sock, &c, 1) == 1;
}

bool recv_byte(int sock, char &c) {
    return recv_all(sock, &c, 1) == 1;
}

string int_fixed(int value, int width) {
    if (value < 0) value = 0;
    string s = to_string(value);
    if ((int)s.size() > width) {
        s = s.substr(s.size() - width); 
    } else if ((int)s.size() < width) {
        s = string(width - s.size(), '0') + s;
    }
    return s;
}

int parse_int_fixed(const string &s) {
    return atoi(s.c_str());
}

bool send_string_len(int sock, const string &s) {
    unsigned char len = (unsigned char)s.size();
    if (send_all(sock, &len, 1) != 1) return false;
    if (len > 0 && send_all(sock, s.data(), (size_t)len) != (ssize_t)len) return false;
    return true;
}

bool recv_string_len(int sock, string &s) {
    unsigned char len = 0;
    if (recv_all(sock, &len, 1) != 1) return false;
    s.resize(len);
    if (len > 0 && recv_all(sock, &s[0], (size_t)len) != (ssize_t)len) return false;
    return true;
}

bool recv_line(int sock, string &line, size_t maxLen = 100 * 1024) {
    line.clear();
    char c;
    while (line.size() < maxLen) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return false;
        line.push_back(c);
        if (c == '\n') break;
    }
    return true;
}

bool send_line(int sock, const string &line) {
    return send_all(sock, line.data(), line.size()) == (ssize_t)line.size();
}


struct MatrixText {
    string name;
    int rows;
    int cols;
    vector<string> rowLines; 
};


bool load_matrix_from_file(const string &filename, MatrixText &A) {
    ifstream in(filename);
    if (!in) {
        cerr << "[CLIENT] No se pudo abrir " << filename << endl;
        return false;
    }

    vector<string> lines;
    string line;

    while (std::getline(in, line)) {
        bool has_non_space = false;
        for (char c : line) {
            if (!isspace(static_cast<unsigned char>(c))) {
                has_non_space = true;
                break;
            }
        }
        if (!has_non_space) continue;

        if (line.empty() || line.back() != '\n')
            line.push_back('\n');

        lines.push_back(line);
    }

    if (lines.empty()) {
        cerr << "[CLIENT] El archivo " << filename << " no contiene filas de matriz\n";
        return false;
    }

    string first = lines[0];
    if (!first.empty() && first.back() == '\n')
        first.pop_back();

    string token;
    istringstream iss(first);
    int cols = 0;
    while (iss >> token) {
        ++cols;
    }

    if (cols == 0) {
        cerr << "[CLIENT] No se encontraron columnas en la primera fila\n";
        return false;
    }

    for (size_t i = 1; i < lines.size(); ++i) {
        string tmp = lines[i];
        if (!tmp.empty() && tmp.back() == '\n')
            tmp.pop_back();
        istringstream iss_row(tmp);
        int count = 0;
        while (iss_row >> token) {
            ++count;
        }
        if (count != cols) {
            cerr << "[CLIENT] Advertencia: la fila " << i
                 << " tiene " << count << " columnas, se esperaban "
                 << cols << endl;
        }
    }

    A.name = "A";
    A.rows = (int)lines.size();
    A.cols = cols;
    A.rowLines = std::move(lines);

    cout << "[CLIENT] Matriz cargada de " << filename
         << ": " << A.rows << "x" << A.cols << endl;
    return true;
}

bool recv_matrix_to_file(int sock, const std::string &outFilename) {
    char c1, c2;

    // ca – cabecera de matriz
    if (!recv_byte(sock, c1)) return false;
    if (!recv_byte(sock, c2)) return false;
    if (c1 != 'c' || c2 != 'a') {
        std::cerr << "[CLIENT] Esperaba 'ca', recibi: " << c1 << c2 << std::endl;
        return false;
    }

    std::string name;
    if (!recv_string_len(sock, name)) return false;

    char buf[6];
    if (recv_all(sock, buf, 6) != 6) return false;
    int filas = parse_int_fixed(std::string(buf, 6));

    if (recv_all(sock, buf, 6) != 6) return false;
    int cols = parse_int_fixed(std::string(buf, 6));

    std::cout << "[CLIENT] Recibido ca: matriz=" << name
              << " filas=" << filas << " columnas=" << cols << std::endl;

    std::ofstream out(outFilename.c_str());
    if (!out) {
        std::cerr << "[CLIENT] No se pudo abrir " << outFilename
                  << " para escribir\n";
        return false;
    }

    // ma – filas de la matriz
    for (int i = 0; i < filas; ++i) {
        if (!recv_byte(sock, c1)) return false;
        if (!recv_byte(sock, c2)) return false;
        if (c1 != 'm' || c2 != 'a') {
            std::cerr << "[CLIENT] Esperaba 'ma' para la fila " << i << std::endl;
            return false;
        }

        std::string name2;
        if (!recv_string_len(sock, name2)) return false;

        if (recv_all(sock, buf, 6) != 6) return false;
        int filaIdx = parse_int_fixed(std::string(buf, 6));

        std::string line;
        if (!recv_line(sock, line)) return false;

        out << line;  

        std::cout << "[CLIENT] Fila " << filaIdx
                  << " de matriz " << name << " recibida.\n";
    }

    out.close();
    std::cout << "[CLIENT] Matriz " << "guardada en '" << outFilename << "'.\n";
    return true;
}


// ===================== Envío Cliente → Master =====================

// 5) N: número de workers
bool send_N(int sock, int numWorkers) {
    if (!send_byte(sock, 'N')) return false;
    string s = int_fixed(numWorkers, 4);
    if (send_all(sock, s.data(), 4) != 4) return false;

    cout << "[CLIENT] Enviado N=" << numWorkers << endl;
    return true;
}

// 6) CA: cabecera de A
bool send_CA(int sock, const MatrixText &A) {
    if (!send_byte(sock, 'C')) return false;
    if (!send_byte(sock, 'A')) return false;

    if (!send_string_len(sock, A.name)) return false;

    string sfil = int_fixed(A.rows, 6);
    string scol = int_fixed(A.cols, 6);

    if (send_all(sock, sfil.data(), 6) != 6) return false;
    if (send_all(sock, scol.data(), 6) != 6) return false;

    cout << "[CLIENT] Enviado CA: matriz=" << A.name
         << " filas=" << A.rows << " columnas=" << A.cols << endl;
    return true;
}

// 7) MA: filas de A
bool send_MA_all_rows(int sock, const MatrixText &A) {
    for (int i = 0; i < A.rows; ++i) {
        if (!send_byte(sock, 'M')) return false;
        if (!send_byte(sock, 'A')) return false;

        if (!send_string_len(sock, A.name)) return false;

        string sfila = int_fixed(i, 6);
        if (send_all(sock, sfila.data(), 6) != 6) return false;

        if (!send_line(sock, A.rowLines[i])) return false;
    }

    cout << "[CLIENT] Enviadas " << A.rows << " filas (MA)\n";
    return true;
}

// ===================== Recepción de resultado (Master → Cliente) =====================


bool recv_result_R(int sock) {
    char c1, c2;

    if (!recv_byte(sock, c1)) return false;
    if (!recv_byte(sock, c2)) return false;
    if (c1 != 'c' || c2 != 'a') {
        cerr << "[CLIENT] Esperaba 'ca', recibi: " << c1 << c2 << endl;
        return false;
    }

    string name;
    if (!recv_string_len(sock, name)) return false;

    char buf[6];
    if (recv_all(sock, buf, 6) != 6) return false;
    int filas = parse_int_fixed(string(buf, 6));

    if (recv_all(sock, buf, 6) != 6) return false;
    int cols = parse_int_fixed(string(buf, 6));

    cout << "[CLIENT] Recibido ca: matriz=" << name
         << " filas=" << filas << " columnas=" << cols << endl;

    ofstream out("resultado_R.txt");
    if (!out) {
        cerr << "[CLIENT] No se pudo abrir resultado_R.txt para escribir\n";
        return false;
    }

    for (int i = 0; i < filas; ++i) {
        if (!recv_byte(sock, c1)) return false;
        if (!recv_byte(sock, c2)) return false;
        if (c1 != 'm' || c2 != 'a') {
            cerr << "[CLIENT] Esperaba 'ma' para la fila " << i << endl;
            return false;
        }

        string name2;
        if (!recv_string_len(sock, name2)) return false;

        if (recv_all(sock, buf, 6) != 6) return false;
        int filaIdx = parse_int_fixed(string(buf, 6));

        string line;
        if (!recv_line(sock, line)) return false;

        out << line; 

        cout << "[CLIENT] Fila " << filaIdx << " de R recibida.\n";
    }

    out.close();
    cout << "[CLIENT] Todas las filas de R recibidas.\n";
    cout << "[CLIENT] Matriz R guardada en 'resultado_R.txt'.\n";
    return true;
}

int main() {
    MatrixText A;
    if (!load_matrix_from_file("matriz.txt", A)) {
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("[CLIENT] socket");
        return 1;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PUERTO_CLIENTE);
    inet_aton(MASTER_IP, &addr.sin_addr);

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[CLIENT] connect");
        close(sock);
        return 1;
    }

    cout << "[CLIENT] Conectado al MASTER en " << MASTER_IP
         << ":" << PUERTO_CLIENTE << endl;

    if (!send_N(sock, NUM_WORKERS)) {
        cerr << "[CLIENT] Error enviando N\n";
        close(sock);
        return 1;
    }

    if (!send_CA(sock, A)) {
        cerr << "[CLIENT] Error enviando CA\n";
        close(sock);
        return 1;
    }

    if (!send_MA_all_rows(sock, A)) {
        cerr << "[CLIENT] Error enviando MA\n";
        close(sock);
        return 1;
    }

    if (!recv_result_R(sock)) {
        cerr << "[CLIENT] Error recibiendo resultado\n";
        close(sock);
        return 1;
    }

    if (!recv_matrix_to_file(sock, "resultado_R.txt")) {
        std::cerr << "[CLIENT] Error recibiendo R\n";
        return 1;
    }
    if (!recv_matrix_to_file(sock, "resultado_U.txt")) {
        std::cerr << "[CLIENT] Error recibiendo U\n";
        return 1;
    }
    if (!recv_matrix_to_file(sock, "resultado_S.txt")) {
        std::cerr << "[CLIENT] Error recibiendo S\n";
        return 1;
    }
    if (!recv_matrix_to_file(sock, "resultado_Vt.txt")) {
        std::cerr << "[CLIENT] Error recibiendo Vt\n";
        return 1;
    }


    shutdown(sock, SHUT_RDWR);
    close(sock);

    cout << "[CLIENT] Terminado.\n";
    return 0;
}

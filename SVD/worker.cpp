#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

const char *MASTER_IP = "127.0.0.1";
const int   PUERTO_WORKERS = 48000;

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

bool matrixText_to_dense(const MatrixText &A, vector<vector<double>> &dense) {
    dense.assign(A.rows, vector<double>(A.cols, 0.0));
    for (int i = 0; i < A.rows; ++i) {
        string line = A.rowLines[i];
        if (!line.empty() && line.back() == '\n') line.pop_back();
        istringstream iss(line);
        for (int j = 0; j < A.cols; ++j) {
            if (!(iss >> dense[i][j])) {
                cerr << "[WORKER] Error parseando valor en fila " << i
                     << ", columna " << j << endl;
                return false;
            }
        }
    }
    return true;
}

MatrixText dense_to_matrixText(const vector<vector<double>> &M,
                               const string &name) {
    MatrixText R;
    R.name = name;
    R.rows = (int)M.size();
    R.cols = (R.rows == 0 ? 0 : (int)M[0].size());
    R.rowLines.resize(R.rows);
    for (int i = 0; i < R.rows; ++i) {
        ostringstream oss;
        for (int j = 0; j < R.cols; ++j) {
            if (j > 0) oss << ' ';
            oss << M[i][j];
        }
        oss << '\n';
        R.rowLines[i] = oss.str();
    }
    return R;
}

// recibe CA + MA* y luego CO + MO*
bool recibir_bloque_y_omega(int sock,
                            MatrixText &A_block,
                            MatrixText &Omega_text) {
    char c1, c2;

    // CA
    if (!recv_byte(sock, c1) || !recv_byte(sock, c2)) return false;
    if (c1 != 'C' || c2 != 'A') {
        cerr << "[WORKER] Esperaba 'CA', recibi " << c1 << c2 << endl;
        return false;
    }
    if (!recv_string_len(sock, A_block.name)) return false;
    char buf[6];
    if (recv_all(sock, buf, 6) != 6) return false;
    A_block.rows = parse_int_fixed(string(buf,6));
    if (recv_all(sock, buf, 6) != 6) return false;
    A_block.cols = parse_int_fixed(string(buf,6));
    cout << "[WORKER] Recibido bloque A: " << A_block.rows
         << "x" << A_block.cols << endl;

    A_block.rowLines.assign(A_block.rows, "");
    // MA*
    for (int i = 0; i < A_block.rows; ++i) {
        if (!recv_byte(sock, c1) || !recv_byte(sock, c2)) return false;
        if (c1 != 'M' || c2 != 'A') {
            cerr << "[WORKER] Esperaba 'MA', recibi " << c1 << c2 << endl;
            return false;
        }
        string nameFila;
        if (!recv_string_len(sock, nameFila)) return false;
        if (recv_all(sock, buf, 6) != 6) return false;
        int filaIdx = parse_int_fixed(string(buf,6));
        string line;
        if (!recv_line(sock, line)) return false;
        if (filaIdx < 0 || filaIdx >= A_block.rows) {
            cerr << "[WORKER] filaIdx A_block fuera de rango\n";
            return false;
        }
        A_block.rowLines[filaIdx] = line;
        cout << "[WORKER][PROTO-RECV] MA" << int_fixed(filaIdx,6)
             << " -> " << line;
    }

    // CO
    if (!recv_byte(sock, c1) || !recv_byte(sock, c2)) return false;
    if (c1 != 'C' || c2 != 'O') {
        cerr << "[WORKER] Esperaba 'CO', recibi " << c1 << c2 << endl;
        return false;
    }
    if (!recv_string_len(sock, Omega_text.name)) return false;
    if (recv_all(sock, buf, 6) != 6) return false;
    Omega_text.rows = parse_int_fixed(string(buf,6));
    if (recv_all(sock, buf, 6) != 6) return false;
    Omega_text.cols = parse_int_fixed(string(buf,6));
    cout << "[WORKER] Recibida Omega: " << Omega_text.rows
         << "x" << Omega_text.cols << endl;

    Omega_text.rowLines.assign(Omega_text.rows, "");
    // MO*
    for (int i = 0; i < Omega_text.rows; ++i) {
        if (!recv_byte(sock, c1) || !recv_byte(sock, c2)) return false;
        if (c1 != 'M' || c2 != 'O') {
            cerr << "[WORKER] Esperaba 'MO', recibi " << c1 << c2 << endl;
            return false;
        }
        string nameFila;
        if (!recv_string_len(sock, nameFila)) return false;
        if (recv_all(sock, buf, 6) != 6) return false;
        int filaIdx = parse_int_fixed(string(buf,6));
        string line;
        if (!recv_line(sock, line)) return false;
        if (filaIdx < 0 || filaIdx >= Omega_text.rows) {
            cerr << "[WORKER] filaIdx Omega fuera de rango\n";
            return false;
        }
        Omega_text.rowLines[filaIdx] = line;
        cout << "[WORKER][PROTO-RECV] MO" << int_fixed(filaIdx,6)
             << " -> " << line;
    }

    return true;
}

// cY + mY*
bool enviar_Y_block(int sock, const MatrixText &Y_block) {
    if (!send_byte(sock, 'c') || !send_byte(sock, 'Y')) return false;
    if (!send_string_len(sock, Y_block.name)) return false;
    string sfil = int_fixed(Y_block.rows, 6);
    string scol = int_fixed(Y_block.cols, 6);
    if (send_all(sock, sfil.data(), 6) != 6) return false;
    if (send_all(sock, scol.data(), 6) != 6) return false;
    cout << "[WORKER][PROTO-SEND] cY" << sfil << scol
         << " (name=" << Y_block.name << ")\n";

    for (int i = 0; i < Y_block.rows; ++i) {
        if (!send_byte(sock, 'm') || !send_byte(sock, 'Y')) return false;
        if (!send_string_len(sock, Y_block.name)) return false;
        string sfila = int_fixed(i, 6);
        if (send_all(sock, sfila.data(), 6) != 6) return false;
        if (!send_line(sock, Y_block.rowLines[i])) return false;
        cout << "[WORKER][PROTO-SEND] mY" << sfila
             << " -> " << Y_block.rowLines[i];
    }
    return true;
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) { perror("[WORKER] socket"); return 1; }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PUERTO_WORKERS);
    inet_aton(MASTER_IP, &addr.sin_addr);

    cout << "[WORKER] Conectando a MASTER " << MASTER_IP
         << ":" << PUERTO_WORKERS << "...\n";

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[WORKER] connect");
        close(sock);
        return 1;
    }

    cout << "[WORKER] Conectado. Esperando bloque de trabajo...\n";

    while (true) {
        MatrixText A_block, Omega_text;
        if (!recibir_bloque_y_omega(sock, A_block, Omega_text)) {
            cerr << "[WORKER] Error recibiendo bloque/Omega, terminando.\n";
            break;
        }

        vector<vector<double>> A_dense, Omega_dense;
        if (!matrixText_to_dense(A_block, A_dense)) break;
        if (!matrixText_to_dense(Omega_text, Omega_dense)) break;

        int m = (int)A_dense.size();
        int n = (m > 0 ? (int)A_dense[0].size() : 0);
        int n2 = (int)Omega_dense.size();
        int r  = (n2 > 0 ? (int)Omega_dense[0].size() : 0);

        if (n != n2) {
            cerr << "[WORKER] Dimensiones incompatibles A(" << m << "x" << n
                 << ") y Omega(" << n2 << "x" << r << ")\n";
            break;
        }

        cout << "[WORKER] Calculando Y_block = A_block * Omega...\n";
        vector<vector<double>> Y(m, vector<double>(r, 0.0));
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < r; ++j) {
                double sum = 0.0;
                for (int k = 0; k < n; ++k) {
                    sum += A_dense[i][k] * Omega_dense[k][j];
                }
                Y[i][j] = sum;
            }
        }
        cout << "[WORKER] Producto A*Omega bloque listo. Enviando a MASTER...\n";

        MatrixText Y_block = dense_to_matrixText(Y, "Y");
        if (!enviar_Y_block(sock, Y_block)) {
            cerr << "[WORKER] Error enviando Y_block.\n";
            break;
        }

        cout << "[WORKER] Bloque enviado. Finalizando worker.\n";
        break;
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
    cout << "[WORKER] Terminado.\n";
    return 0;
}

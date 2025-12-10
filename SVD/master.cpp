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
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <random>
#include <cmath>
#include <algorithm>

using namespace std;

const int PUERTO_WORKERS = 48000;
const int PUERTO_CLIENTE = 47000;

vector<int> g_workerSockets;
int g_connectedWorkers = 0;
mutex g_workersMtx;

// ================= helpers de red =================

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
                cerr << "[MASTER] Error parseando valor en fila " << i
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


const int RSVD_RANK = 10;

void generate_Omega(int n, int r, vector<vector<double>> &Omega) {
    cout << "[MASTER][RSVD] Generando Omega " << n << "x" << r << "...\n";
    Omega.assign(n, vector<double>(r, 0.0));
    mt19937 gen(12345);
    normal_distribution<double> dist(0.0, 1.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < r; ++j)
            Omega[i][j] = dist(gen);
}

void orthonormalize_Y_to_Q(const vector<vector<double>> &Y,
                           int r,
                           vector<vector<double>> &Q) {
    int m = (int)Y.size();
    Q.assign(m, vector<double>(r, 0.0));

    cout << "[MASTER][RSVD] Ortonormalizando columnas de Y...\n";
    for (int j = 0; j < r; ++j) {
        for (int i = 0; i < m; ++i) Q[i][j] = Y[i][j];

        for (int k = 0; k < j; ++k) {
            double dot = 0.0;
            for (int i = 0; i < m; ++i) dot += Q[i][k] * Q[i][j];
            for (int i = 0; i < m; ++i) Q[i][j] -= dot * Q[i][k];
        }

        double norm2 = 0.0;
        for (int i = 0; i < m; ++i) norm2 += Q[i][j] * Q[i][j];
        double norm = sqrt(norm2);
        if (norm < 1e-12) {
            for (int i = 0; i < m; ++i) Q[i][j] = 0.0;
            cout << "[MASTER][RSVD] Columna " << j << " ~0.\n";
        } else {
            double inv = 1.0 / norm;
            for (int i = 0; i < m; ++i) Q[i][j] *= inv;
        }
    }
}

void jacobi_eigen_symmetric(const vector<vector<double>> &A,
                            vector<double> &eigenvalues,
                            vector<vector<double>> &eigenvectors) {
    int n = (int)A.size();
    vector<vector<double>> D = A;
    eigenvectors.assign(n, vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) eigenvectors[i][i] = 1.0;

    const int maxIter = 100;
    const double tol = 1e-10;

    for (int iter = 0; iter < maxIter; ++iter) {
        int p = 0, q = 1;
        double maxOff = 0.0;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double val = fabs(D[i][j]);
                if (val > maxOff) {
                    maxOff = val;
                    p = i; q = j;
                }
            }
        }
        if (maxOff < tol) break;

        double app = D[p][p];
        double aqq = D[q][q];
        double apq = D[p][q];

        double phi = 0.5 * atan2(2.0 * apq, (aqq - app));
        double c = cos(phi);
        double s = sin(phi);

        // rotar D
        for (int k = 0; k < n; ++k) {
            if (k == p || k == q) continue;
            double Dkp = D[k][p];
            double Dkq = D[k][q];
            D[k][p] = c * Dkp - s * Dkq;
            D[p][k] = D[k][p];
            D[k][q] = c * Dkq + s * Dkp;
            D[q][k] = D[k][q];
        }

        double app_new = c * c * app - 2.0 * s * c * apq + s * s * aqq;
        double aqq_new = s * s * app + 2.0 * s * c * apq + c * c * aqq;
        D[p][p] = app_new;
        D[q][q] = aqq_new;
        D[p][q] = 0.0;
        D[q][p] = 0.0;

        // rotar eigenvectors
        for (int k = 0; k < n; ++k) {
            double vip = eigenvectors[k][p];
            double viq = eigenvectors[k][q];
            eigenvectors[k][p] = c * vip - s * viq;
            eigenvectors[k][q] = s * vip + c * viq;
        }
    }

    eigenvalues.assign(n, 0.0);
    for (int i = 0; i < n; ++i) eigenvalues[i] = D[i][i];
}

void rsvd_full_from_Y(const vector<vector<double>> &A,
                      const vector<vector<double>> &Y,
                      int rank,
                      vector<vector<double>> &R,
                      vector<vector<double>> &U_full,
                      vector<vector<double>> &S_mat,
                      vector<vector<double>> &Vt) {
    int m = (int)A.size();
    if (m == 0) { R.clear(); U_full.clear(); S_mat.clear(); Vt.clear(); return; }
    int n = (int)A[0].size();
    int r = rank;
    if (r > m) r = m;
    if (r > n) r = n;
    if (r <= 0) { R = A; U_full.clear(); S_mat.clear(); Vt.clear(); return; }

    // Q
    vector<vector<double>> Q;
    orthonormalize_Y_to_Q(Y, r, Q);

    // B = Q^T A (r×n)
    cout << "[MASTER][RSVD] Calculando B = Q^T * A...\n";
    vector<vector<double>> B(r, vector<double>(n, 0.0));
    for (int i = 0; i < r; ++i) {         // fila de B
        for (int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (int k = 0; k < m; ++k) sum += Q[k][i] * A[k][j];
            B[i][j] = sum;
        }
    }

    // C = B * B^T (r×r)
    cout << "[MASTER][RSVD] Calculando C = B * B^T para eigen...\n";
    vector<vector<double>> C(r, vector<double>(r, 0.0));
    for (int i = 0; i < r; ++i) {
        for (int j = i; j < r; ++j) {
            double sum = 0.0;
            for (int k = 0; k < n; ++k) sum += B[i][k] * B[j][k];
            C[i][j] = sum;
            C[j][i] = sum;
        }
    }

    // eigen C
    vector<double> evals;
    vector<vector<double>> evecs;
    jacobi_eigen_symmetric(C, evals, evecs);

    // ordenar eigenvalores de mayor a menor
    vector<int> idx(r);
    for (int i = 0; i < r; ++i) idx[i] = i;
    sort(idx.begin(), idx.end(), [&](int a, int b) {
        return evals[a] > evals[b];
    });

    vector<double> sigma(r, 0.0);
    vector<vector<double>> U_hat(r, vector<double>(r, 0.0));
    for (int k = 0; k < r; ++k) {
        int j = idx[k];
        double lambda = evals[j];
        if (lambda < 0.0) lambda = 0.0;
        sigma[k] = sqrt(lambda);
        for (int i = 0; i < r; ++i) {
            U_hat[i][k] = evecs[i][j];
        }
    }

    // U_full = Q * U_hat (m×r)
    cout << "[MASTER][RSVD] Calculando U = Q * U_hat...\n";
    U_full.assign(m, vector<double>(r, 0.0));
    for (int i = 0; i < m; ++i) {
        for (int k = 0; k < r; ++k) {
            double sum = 0.0;
            for (int j = 0; j < r; ++j) sum += Q[i][j] * U_hat[j][k];
            U_full[i][k] = sum;
        }
    }

    // S diagonal
    cout << "[MASTER][RSVD] Construyendo matriz S (diagonal)...\n";
    S_mat.assign(r, vector<double>(r, 0.0));
    for (int i = 0; i < r; ++i) S_mat[i][i] = sigma[i];

    // Vt: r×n, filas = vectores singulares derechos transpuestos
    cout << "[MASTER][RSVD] Calculando Vt...\n";
    Vt.assign(r, vector<double>(n, 0.0));
    const double eps = 1e-12;
    for (int k = 0; k < r; ++k) {
        double sig = sigma[k];
        if (sig < eps) continue;
        for (int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (int i = 0; i < r; ++i) sum += U_hat[i][k] * B[i][j];
            Vt[k][j] = sum / sig;
        }
    }

    // R = U * S * Vt
    cout << "[MASTER][RSVD] Calculando R = U * S * Vt...\n";
    vector<vector<double>> US(m, vector<double>(r, 0.0));
    for (int i = 0; i < m; ++i) {
        for (int k = 0; k < r; ++k) {
            US[i][k] = U_full[i][k] * sigma[k];
        }
    }
    R.assign(m, vector<double>(n, 0.0));
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            double sum = 0.0;
            for (int k = 0; k < r; ++k) sum += US[i][k] * Vt[k][j];
            R[i][j] = sum;
        }
    }

    cout << "[MASTER][RSVD] RSVD completo calculado.\n";
}

// ================ RSVD distribuido (Y en workers) =================

bool send_block_to_worker_and_recvY(int workerSock,
                                    const MatrixText &A_block,
                                    const vector<vector<double>> &Omega,
                                    int rank,
                                    int globalRowStart,
                                    vector<vector<double>> &Y_dense) {
    int blockRows = A_block.rows;
    int n = A_block.cols;
    int r = rank;

    // CA (A_block)
    if (!send_byte(workerSock, 'C') || !send_byte(workerSock, 'A')) return false;
    if (!send_string_len(workerSock, A_block.name)) return false;
    string sfil = int_fixed(blockRows, 6);
    string scol = int_fixed(n, 6);
    if (send_all(workerSock, sfil.data(), 6) != 6) return false;
    if (send_all(workerSock, scol.data(), 6) != 6) return false;
    cout << "[MASTER][PROTO-SEND] CA" << sfil << scol << " (A_block)\n";

    // MA*
    for (int i = 0; i < blockRows; ++i) {
        if (!send_byte(workerSock, 'M') || !send_byte(workerSock, 'A')) return false;
        if (!send_string_len(workerSock, A_block.name)) return false;
        string sfila = int_fixed(i, 6);
        if (send_all(workerSock, sfila.data(), 6) != 6) return false;
        if (!send_line(workerSock, A_block.rowLines[i])) return false;
        cout << "[MASTER][PROTO-SEND] MA" << sfila << " -> " << A_block.rowLines[i];
    }

    // CO (Omega)
    if (!send_byte(workerSock, 'C') || !send_byte(workerSock, 'O')) return false;
    string nameO = "O";
    if (!send_string_len(workerSock, nameO)) return false;
    string sfilO = int_fixed(n, 6);
    string scolO = int_fixed(r, 6);
    if (send_all(workerSock, sfilO.data(), 6) != 6) return false;
    if (send_all(workerSock, scolO.data(), 6) != 6) return false;
    cout << "[MASTER][PROTO-SEND] CO" << sfilO << scolO << " (Omega)\n";

    // MO*
    for (int i = 0; i < n; ++i) {
        if (!send_byte(workerSock, 'M') || !send_byte(workerSock, 'O')) return false;
        if (!send_string_len(workerSock, nameO)) return false;
        string sfila = int_fixed(i, 6);
        if (send_all(workerSock, sfila.data(), 6) != 6) return false;
        ostringstream oss;
        for (int j = 0; j < r; ++j) {
            if (j > 0) oss << ' ';
            oss << Omega[i][j];
        }
        oss << '\n';
        string line = oss.str();
        if (!send_line(workerSock, line)) return false;
        cout << "[MASTER][PROTO-SEND] MO" << sfila << " -> " << line;
    }

    // Recibir cY
    char c1, c2;
    if (!recv_byte(workerSock, c1) || !recv_byte(workerSock, c2)) return false;
    if (c1 != 'c' || c2 != 'Y') {
        cerr << "[MASTER] Esperaba 'cY', recibi " << c1 << c2 << endl;
        return false;
    }
    string nameY;
    if (!recv_string_len(workerSock, nameY)) return false;
    char buf[6];
    if (recv_all(workerSock, buf, 6) != 6) return false;
    int filasY = parse_int_fixed(string(buf, 6));
    if (recv_all(workerSock, buf, 6) != 6) return false;
    int colsY = parse_int_fixed(string(buf, 6));
    cout << "[MASTER][PROTO-RECV] cY" << int_fixed(filasY,6)
         << int_fixed(colsY,6) << " (name=" << nameY << ")\n";
    if (filasY != blockRows || colsY != r) {
        cerr << "[MASTER] Dimensiones Y_block incorrectas\n";
        return false;
    }

    // mY*
    MatrixText Y_block;
    Y_block.name = nameY;
    Y_block.rows = filasY;
    Y_block.cols = colsY;
    Y_block.rowLines.assign(filasY, "");
    for (int i = 0; i < filasY; ++i) {
        if (!recv_byte(workerSock, c1) || !recv_byte(workerSock, c2)) return false;
        if (c1 != 'm' || c2 != 'Y') {
            cerr << "[MASTER] Esperaba 'mY', recibi " << c1 << c2 << endl;
            return false;
        }
        string name2;
        if (!recv_string_len(workerSock, name2)) return false;
        if (recv_all(workerSock, buf, 6) != 6) return false;
        int filaIdx = parse_int_fixed(string(buf, 6));
        string line;
        if (!recv_line(workerSock, line)) return false;
        if (filaIdx < 0 || filaIdx >= filasY) {
            cerr << "[MASTER] filaIdx Y fuera de rango\n";
            return false;
        }
        Y_block.rowLines[filaIdx] = line;
        cout << "[MASTER][PROTO-RECV] mY" << int_fixed(filaIdx,6)
             << " -> " << line;
    }

    vector<vector<double>> Y_block_dense;
    if (!matrixText_to_dense(Y_block, Y_block_dense)) return false;
    for (int i = 0; i < filasY; ++i)
        for (int j = 0; j < r; ++j)
            Y_dense[globalRowStart + i][j] = Y_block_dense[i][j];

    cout << "[MASTER][RSVD] Recibido Y_block para filas globales ["
         << globalRowStart << "," << (globalRowStart + filasY) << ")\n";
    return true;
}

bool rsvd_distributed(const MatrixText &A_text,
                      const vector<vector<double>> &A_dense,
                      int numWorkers,
                      vector<vector<double>> &R,
                      vector<vector<double>> &U_full,
                      vector<vector<double>> &S_mat,
                      vector<vector<double>> &Vt) {
    int m = A_text.rows;
    if (m == 0) { R.clear(); U_full.clear(); S_mat.clear(); Vt.clear(); return true; }
    int n = A_text.cols;
    int r = RSVD_RANK;

    cout << "[MASTER][RSVD] RSVD distribuido: m=" << m << ", n=" << n
         << ", r=" << r << ", workers=" << numWorkers << endl;

    vector<vector<double>> Omega;
    generate_Omega(n, r, Omega);

    vector<vector<double>> Y_dense(m, vector<double>(r, 0.0));
    int usedWorkers = min(numWorkers, m);
    if (usedWorkers <= 0) {
        cerr << "[MASTER][RSVD] Sin workers, no se distribuye.\n";
        return false;
    }

    for (int w = 0; w < usedWorkers; ++w) {
        int start = (w * m) / usedWorkers;
        int end   = ((w + 1) * m) / usedWorkers;
        int blockRows = end - start;
        if (blockRows <= 0) continue;

        MatrixText A_block;
        A_block.name = "A";
        A_block.rows = blockRows;
        A_block.cols = n;
        A_block.rowLines.resize(blockRows);
        for (int i = 0; i < blockRows; ++i) {
            A_block.rowLines[i] = A_text.rowLines[start + i];
        }

        int sock;
        {
            lock_guard<mutex> lk(g_workersMtx);
            if (w >= (int)g_workerSockets.size()) {
                cerr << "[MASTER][RSVD] No hay socket para worker " << w << endl;
                return false;
            }
            sock = g_workerSockets[w];
        }

        cout << "[MASTER][RSVD] Enviando bloque filas [" << start << "," << end
             << ") al WORKER " << w << "...\n";

        if (!send_block_to_worker_and_recvY(sock, A_block, Omega, r, start, Y_dense)) {
            cerr << "[MASTER][RSVD] Error con worker " << w << endl;
            return false;
        }
    }

    rsvd_full_from_Y(A_dense, Y_dense, r, R, U_full, S_mat, Vt);
    return true;
}

// ================ Protocolo Cliente-Master (N, CA, MA, ca, ma) =================

bool recv_N(int sock, int &numWorkersSolicitados) {
    char cmd;
    if (!recv_byte(sock, cmd)) return false;
    if (cmd != 'N') {
        cerr << "[MASTER] Esperaba 'N', recibi: " << cmd << endl;
        return false;
    }
    char buf[4];
    if (recv_all(sock, buf, 4) != 4) return false;
    numWorkersSolicitados = parse_int_fixed(string(buf, 4));
    cout << "[MASTER][PROTO-RECV] N" << string(buf,4)
         << " (N=" << numWorkersSolicitados << ")\n";
    return true;
}

bool recv_CA(int sock, MatrixText &A) {
    char c1, c2;
    if (!recv_byte(sock, c1) || !recv_byte(sock, c2)) return false;
    if (c1 != 'C' || c2 != 'A') {
        cerr << "[MASTER] Esperaba 'CA', recibi: " << c1 << c2 << endl;
        return false;
    }
    if (!recv_string_len(sock, A.name)) return false;
    char buf[6];
    if (recv_all(sock, buf, 6) != 6) return false;
    A.rows = parse_int_fixed(string(buf,6));
    if (recv_all(sock, buf, 6) != 6) return false;
    A.cols = parse_int_fixed(string(buf,6));
    cout << "[MASTER][PROTO-RECV] CA" << int_fixed(A.rows,6)
         << int_fixed(A.cols,6) << " (name=" << A.name << ")\n";
    return true;
}

bool recv_MA_all_rows(int sock, MatrixText &A) {
    A.rowLines.assign(A.rows, "");
    for (int i = 0; i < A.rows; ++i) {
        char c1, c2;
        if (!recv_byte(sock, c1) || !recv_byte(sock, c2)) return false;
        if (c1 != 'M' || c2 != 'A') {
            cerr << "[MASTER] Esperaba 'MA', recibi: " << c1 << c2 << endl;
            return false;
        }
        string nameFila;
        if (!recv_string_len(sock, nameFila)) return false;
        char buf[6];
        if (recv_all(sock, buf, 6) != 6) return false;
        int filaIdx = parse_int_fixed(string(buf,6));
        string line;
        if (!recv_line(sock, line)) return false;
        if (filaIdx < 0 || filaIdx >= A.rows) {
            cerr << "[MASTER] filaIdx fuera de rango\n";
            return false;
        }
        A.rowLines[filaIdx] = line;
        cout << "[MASTER][PROTO-RECV] MA" << int_fixed(filaIdx,6)
             << " -> " << line;
    }
    cout << "[MASTER] Recibidas todas las filas de A.\n";
    return true;
}

bool send_ca(int sock, const MatrixText &M) {
    if (!send_byte(sock, 'c') || !send_byte(sock, 'a')) return false;
    if (!send_string_len(sock, M.name)) return false;
    string sfil = int_fixed(M.rows,6);
    string scol = int_fixed(M.cols,6);
    if (send_all(sock, sfil.data(),6) != 6) return false;
    if (send_all(sock, scol.data(),6) != 6) return false;
    cout << "[MASTER][PROTO-SEND] ca" << sfil << scol
         << " (name=" << M.name << ")\n";
    return true;
}

bool send_ma_all_rows(int sock, const MatrixText &M) {
    for (int i = 0; i < M.rows; ++i) {
        if (!send_byte(sock, 'm') || !send_byte(sock, 'a')) return false;
        if (!send_string_len(sock, M.name)) return false;
        string sfila = int_fixed(i,6);
        if (send_all(sock, sfila.data(),6) != 6) return false;
        if (!send_line(sock, M.rowLines[i])) return false;
        cout << "[MASTER][PROTO-SEND] ma" << sfila
             << " -> " << M.rowLines[i];
    }
    cout << "[MASTER] Enviadas todas las filas de " << M.name << ".\n";
    return true;
}

// ================ aceptación de workers =================

void aceptar_workers(int listenSock) {
    while (true) {
        sockaddr_in waddr;
        socklen_t wlen = sizeof(waddr);
        int wSock = accept(listenSock, (sockaddr *)&waddr, &wlen);
        if (wSock < 0) {
            perror("[MASTER] accept worker");
            continue;
        }
        {
            lock_guard<mutex> lk(g_workersMtx);
            g_workerSockets.push_back(wSock);
            g_connectedWorkers = (int)g_workerSockets.size();
            cout << "[MASTER] WORKER conectado desde "
                 << inet_ntoa(waddr.sin_addr) << ":"
                 << ntohs(waddr.sin_port)
                 << ". Total workers=" << g_connectedWorkers << endl;
        }
    }
}

// ================ flujo Cliente–Master =================

bool manejar_cliente(int clientSock) {
    int expectedWorkers = 0;
    MatrixText A_text;

    if (!recv_N(clientSock, expectedWorkers)) {
        cerr << "[MASTER] Error recibiendo N\n";
        return false;
    }
    if (!recv_CA(clientSock, A_text)) {
        cerr << "[MASTER] Error recibiendo CA\n";
        return false;
    }
    if (!recv_MA_all_rows(clientSock, A_text)) {
        cerr << "[MASTER] Error recibiendo MA*\n";
        return false;
    }
    cout << "[MASTER] Matriz A recibida: " << A_text.rows
         << "x" << A_text.cols << endl;

    if (expectedWorkers > 0) {
        cout << "[MASTER] Esperando " << expectedWorkers
             << " workers conectados...\n";
        while (true) {
            {
                lock_guard<mutex> lk(g_workersMtx);
                if (g_connectedWorkers >= expectedWorkers) break;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        cout << "[MASTER] Workers conectados: " << g_connectedWorkers << endl;
    }

    vector<vector<double>> A_dense;
    if (!matrixText_to_dense(A_text, A_dense)) return false;

    vector<vector<double>> R_dense, U_dense, S_dense, Vt_dense;
    bool distOk = false;
    if (expectedWorkers > 0 && g_connectedWorkers > 0) {
        distOk = rsvd_distributed(A_text, A_dense,
                                  expectedWorkers,
                                  R_dense, U_dense, S_dense, Vt_dense);
    }

    if (!distOk) {
        cout << "[MASTER][RSVD] Fallback RSVD local.\n";
        int m = (int)A_dense.size();
        if (m == 0) {
            R_dense.clear(); U_dense.clear(); S_dense.clear(); Vt_dense.clear();
        } else {
            int n = (int)A_dense[0].size();
            int r = RSVD_RANK;
            if (r > m) r = m;
            if (r > n) r = n;
            vector<vector<double>> Omega;
            generate_Omega(n, r, Omega);
            vector<vector<double>> Y(m, vector<double>(r,0.0));
            cout << "[MASTER][RSVD] Calculando Y = A * Omega local...\n";
            for (int i = 0; i < m; ++i)
                for (int j = 0; j < r; ++j) {
                    double sum = 0.0;
                    for (int k = 0; k < n; ++k) sum += A_dense[i][k]*Omega[k][j];
                    Y[i][j] = sum;
                }
            rsvd_full_from_Y(A_dense, Y, r, R_dense, U_dense, S_dense, Vt_dense);
        }
    }

    MatrixText R_text  = dense_to_matrixText(R_dense,  "R");
    MatrixText U_text  = dense_to_matrixText(U_dense,  "U");
    MatrixText S_text  = dense_to_matrixText(S_dense,  "S");
    MatrixText Vt_text = dense_to_matrixText(Vt_dense, "Vt");

    cout << "[MASTER][RESULT] Matriz R (" << R_text.rows << "x"
         << R_text.cols << "):\n";
    for (int i = 0; i < R_text.rows; ++i) {
        cout << "[MASTER][RESULT] R[" << i << "]: "
             << R_text.rowLines[i];
    }

    if (!send_ca(clientSock, R_text)) return false;
    if (!send_ma_all_rows(clientSock, R_text)) return false;

    if (!send_ca(clientSock, U_text)) return false;
    if (!send_ma_all_rows(clientSock, U_text)) return false;

    if (!send_ca(clientSock, S_text)) return false;
    if (!send_ma_all_rows(clientSock, S_text)) return false;

    if (!send_ca(clientSock, Vt_text)) return false;
    if (!send_ma_all_rows(clientSock, Vt_text)) return false;

    cout << "[MASTER] Flujo Cliente–Master terminado.\n";
    return true;
}

// ================ main =================

int main() {
    // workers
    int listenWorkers = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenWorkers < 0) { perror("socket workers"); return 1; }
    int opt = 1;
    setsockopt(listenWorkers, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addrWorkers;
    memset(&addrWorkers, 0, sizeof(addrWorkers));
    addrWorkers.sin_family = AF_INET;
    addrWorkers.sin_port = htons(PUERTO_WORKERS);
    addrWorkers.sin_addr.s_addr = INADDR_ANY;
    if (bind(listenWorkers, (sockaddr *)&addrWorkers, sizeof(addrWorkers)) < 0) {
        perror("bind workers"); close(listenWorkers); return 1;
    }
    if (listen(listenWorkers, 10) < 0) {
        perror("listen workers"); close(listenWorkers); return 1;
    }
    cout << "[MASTER] Escuchando workers en puerto " << PUERTO_WORKERS << endl;
    thread tWorkers(aceptar_workers, listenWorkers);
    tWorkers.detach();

    // cliente
    int listenClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenClient < 0) { perror("socket client"); close(listenWorkers); return 1; }
    setsockopt(listenClient, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addrClient;
    memset(&addrClient, 0, sizeof(addrClient));
    addrClient.sin_family = AF_INET;
    addrClient.sin_port = htons(PUERTO_CLIENTE);
    addrClient.sin_addr.s_addr = INADDR_ANY;
    if (bind(listenClient, (sockaddr *)&addrClient, sizeof(addrClient)) < 0) {
        perror("bind client"); close(listenWorkers); close(listenClient); return 1;
    }
    if (listen(listenClient, 1) < 0) {
        perror("listen client"); close(listenWorkers); close(listenClient); return 1;
    }
    cout << "[MASTER] Esperando CLIENTE en puerto " << PUERTO_CLIENTE << "...\n";

    sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int clientSock = accept(listenClient, (sockaddr *)&caddr, &clen);
    if (clientSock < 0) {
        perror("accept client"); close(listenWorkers); close(listenClient); return 1;
    }
    cout << "[MASTER] CLIENTE conectado.\n";

    manejar_cliente(clientSock);

    shutdown(clientSock, SHUT_RDWR);
    close(clientSock);
    close(listenClient);
    close(listenWorkers);
    cout << "[MASTER] Servidor terminado.\n";
    return 0;
}

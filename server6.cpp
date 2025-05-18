#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <map>
#include <iostream>
#include <thread>
#include <vector>
#include <list>
#include <algorithm>
#include <chrono>

using namespace std;

// Game constants
const int BOARD_SIZE = 9;
const int WINNING_LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // rows
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // columns
    {0, 4, 8}, {2, 4, 6}             // diagonals
};

// Game state
struct GameState {
    char board[BOARD_SIZE] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    vector<string> players;
    vector<string> spectators;
    int currentPlayerIndex = 0;
    int movesCount = 0;
    
    void reset() {
        fill(begin(board), end(board), ' ');
        players.clear();
        spectators.clear();
        currentPlayerIndex = 0;
        movesCount = 0;
    }
    
    bool isValidMove(int position) {
        return position >= 0 && position < BOARD_SIZE && board[position] == ' ';
    }
    
    bool checkWinner() {
        for (const auto& line : WINNING_LINES) {
            if (board[line[0]] != ' ' && 
                board[line[0]] == board[line[1]] && 
                board[line[1]] == board[line[2]]) {
                return true;
            }
        }
        return false;
    }
    
    bool isDraw() {
        return movesCount == BOARD_SIZE;
    }
};

// Server state
class Server {
    map<string, int> clients;
    GameState game;
    int nextClientId = 0;
    
public:
    void handleClient(int clientSocket, const string& clientName) {
        clients[clientName] = clientSocket;
        
        char buffer[300];
        while (true) {
            int bytesRead = read(clientSocket, buffer, 5);
            if (bytesRead <= 0) break;
            
            buffer[bytesRead] = '\0';
            int messageLength = atoi(buffer);
            
            bytesRead = read(clientSocket, buffer, 1);
            if (bytesRead <= 0) break;

            switch (buffer[0]) {
                case 'L': handleListRequest(clientSocket); break;
                case 'M': handlePrivateMessage(clientSocket, clientName, messageLength); break;
                case 'B': handleBroadcast(clientSocket, clientName, messageLength); break;
                case 'Q': handleQuit(clientSocket, clientName); return;
                case 'J': handleJoinGame(clientSocket, clientName); break;
                case 'V': handleViewGame(clientSocket, clientName); break;
                case 'P': handlePlayerMove(clientSocket, clientName); break;
                case 'F': handleFileTransfer(clientSocket, clientName); break;
                default: break;
            }
        }
        
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
    }
    
private:
    void handleListRequest(int clientSocket) {
        string userList;
        for (const auto& client : clients) {
            userList += client.first + "\n";
        }
        
        char response[256];
        sprintf(response, "%05dl%s", userList.size() + 1, userList.c_str());
        write(clientSocket, response, userList.size() + 6);
    }
    
    void handlePrivateMessage(int clientSocket, const string& sender, int messageLength) {
        // Implementation similar to original
    }
    
    void handleBroadcast(int clientSocket, const string& sender, int messageLength) {
        // Implementation similar to original
    }
    
    void handleQuit(int clientSocket, const string& clientName) {
        printf("\nSE RETIRO : %s", clientName.c_str());
        clients.erase(clientName);
    }
    
    void handleJoinGame(int clientSocket, const string& clientName) {
        if (game.players.size() < 2) {
            game.players.push_back(clientName);
            char symbol = (game.players.size() == 1) ? 'O' : 'X';
            
            string message = (game.players.size() == 1) ? "Wait for please" : "Iniciando";
            sendServerMessage(clientSocket, message);
            
            if (game.players.size() == 2) {
                sendTurnNotification();
            }
        } else {
            sendServerMessage(clientSocket, "Quieres ver?");
        }
    }
    
    void handleViewGame(int clientSocket, const string& clientName) {
        game.spectators.push_back(clientName);
        sendGameState();
    }
    
    void handlePlayerMove(int clientSocket, const string& clientName) {
        // Implementation similar to original
    }
    
    void handleFileTransfer(int clientSocket, const string& sender) {
        // Implementation similar to original
    }
    
    void sendServerMessage(int clientSocket, const string& message) {
        char buffer[256];
        sprintf(buffer, "%05dm%05d%s%05d%s", 
                message.size() + 16, 
                message.size(), message.c_str(), 
                16, "Server TicTacToe");
        write(clientSocket, buffer, message.size() + 26);
    }
    
    void sendTurnNotification() {
        char buffer[20];
        sprintf(buffer, "%05dT%c", 2, game.players[game.currentPlayerIndex][0]);
        write(clients[game.players[game.currentPlayerIndex]], buffer, 7);
    }
    
    void sendGameState() {
        char buffer[20];
        sprintf(buffer, "%05dX%s", 10, game.board);
        
        for (const auto& player : game.players) {
            write(clients[player], buffer, 15);
        }
        
        for (const auto& spectator : game.spectators) {
            write(clients[spectator], buffer, 15);
        }
    }
};

int main() {
    Server server;
    int serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if (serverSocket == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1100);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) {
        perror("error bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 10)) {
        perror("error listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    while (true) {
        int clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket < 0) {
            perror("error accept failed");
            continue;
        }

        char buffer[256];
        int bytesRead = read(clientSocket, buffer, 5);
        if (bytesRead <= 0) {
            close(clientSocket);
            continue;
        }
        
        buffer[bytesRead] = '\0';
        int messageLength = atoi(buffer);
        
        bytesRead = read(clientSocket, buffer, 1);
        if (bytesRead <= 0 || buffer[0] != 'N') {
            close(clientSocket);
            continue;
        }
        
        bytesRead = read(clientSocket, buffer, messageLength - 1);
        if (bytesRead <= 0) {
            close(clientSocket);
            continue;
        }
        
        buffer[bytesRead] = '\0';
        string clientName(buffer);
        
        printf("!!bienvenido!! --> %s\n", clientName.c_str());
        thread([&server, clientSocket, clientName]() {
            server.handleClient(clientSocket, clientName);
        }).detach();
    }

    close(serverSocket);
    return 0;
}

/* Server code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream> // std::cout
#include <thread>   // std::thread, std::this_thread::sleep_for

#include <list>

std::list<int> listSockets;

using namespace std;
void writeSocketThread(int cliSocket)
{
  char buffer[300];
  int tamano;
  do
  {
    printf("Enter a msg:");
    fgets(buffer, 100, stdin);
    tamano = strlen(buffer);
    buffer[tamano] = '\0';
    tamano = write(cliSocket, buffer, tamano);

  } while (strncmp(buffer, "exit", 4) != 0);
  shutdown(cliSocket, SHUT_RDWR);
  close(cliSocket);
}
void readSocketThread(int cliSocket)
{
  char buffer[300];
  do
  {
    /* perform read write operations ... */
    int n = read(cliSocket, buffer, 100);
    buffer[n] = '\0';
    //printf("\n%s\n", buffer);
    //printf("Enter a msg:");
    for (list<int>::iterator it=listSockets.begin(); it != listSockets.end(); ++it)
       write(*it, buffer, n);

  } while (strncmp(buffer, "exit", 4) != 0);
  shutdown(cliSocket, SHUT_RDWR);
  close(cliSocket);
  listSockets.remove(cliSocket);
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
    listSockets.push_back(ClientFD);
    printf("We have a new Cliente!!!!!!!\n");

    std::thread(readSocketThread, ClientFD).detach();
    //std::thread(writeSocketThread, ClientFD).detach();

  }

  close(SocketFD);
  return 0;
}

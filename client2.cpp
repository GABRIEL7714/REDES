/* Client code in C */

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


using namespace std;

void readSocketThread(int cliSocket)
{
  char buffer[300];
  do
  {
    /* perform read write operations ... */
    int n = read(cliSocket, buffer, 100);
    buffer[n] = '\0';
    printf("\n%s\n", buffer);
    printf("Enter a msg:");
  } while (strncmp(buffer, "exit", 4) != 0);
  shutdown(cliSocket, SHUT_RDWR);
  close(cliSocket);
}

int main(void)
{
  struct sockaddr_in stSockAddr;
  int Res;
  int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  int n;
  char buffer[250];

  if (-1 == SocketFD)
  {
    perror("cannot create socket");
    exit(EXIT_FAILURE);
  }

  memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

  stSockAddr.sin_family = AF_INET;
  stSockAddr.sin_port = htons(45000);
  Res = inet_pton(AF_INET, "172.16.19.60", &stSockAddr.sin_addr);

  if (0 > Res)
  {
    perror("error: first parameter is not a valid address family");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }
  else if (0 == Res)
  {
    perror("char string (second parameter does not contain valid ipaddress");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }

  if (-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
  {
    perror("connect failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }
  do
  {
    std::thread(readSocketThread, SocketFD).detach();
    printf("Enter a msg:");
    fgets(buffer, 100, stdin);
    n = strlen(buffer);
    buffer[n] = '\0';
    n = write(SocketFD, buffer, 100);

  } while (strncmp(buffer, "exit", 4) != 0);
  shutdown(SocketFD, SHUT_RDWR);
  close(SocketFD);
  return 0;
}

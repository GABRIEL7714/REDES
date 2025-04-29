/* Client code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include<iostream>
#include <openssl/sha.h> 
using namespace std;
bool controlmsg = 1;
const char* para = "para";
const char* msgg = "msg";
char michi;
void* readSocketThread(void* arg) {
    int cliSocket = *(int*)arg;
    char buffer[300];
    int n;

    while (1) {
        n = read(cliSocket , buffer, 5);
        
        if (n <= 0) {
            printf("\n[Cliente] Conexión cerrada o error. Cerrando hilo...\n");
            return NULL;
            break;
        }
       // printf("entardnobucle");
        buffer[n] = '\0';
        
        int tamano = atoi(buffer);
        
        n = read(cliSocket, buffer, 1);
        if (n <= 0) {return NULL;}

        if (buffer[0] == 'l') {
            buffer[0] = '\0';   
            n = read(cliSocket, buffer, tamano - 1);
            if (n <= 0) break;
            buffer[n] = '\0';
            printf("\nLista de usuarios:\n%s\n", buffer);
        }
        else if (buffer[0] == 'm') {
            char buffer2[100];

            n = read(cliSocket, buffer, 5);
            if (n <= 0) break;
            buffer[n] = '\0';
            tamano = atoi(buffer);

            n = read(cliSocket, buffer, tamano);
            if (n <= 0) break;
            buffer[n] = '\0';

            n = read(cliSocket, buffer2, 5);
            if (n <= 0) break;
            buffer2[n] = '\0';
            tamano = atoi(buffer2);

            n = read(cliSocket, buffer2, tamano);
            if (n <= 0) break;
            buffer2[n] = '\0';

            printf("\nMensaje de <%s>: %s\n", buffer2, buffer);
        }
        else if (buffer[0] == 'b') {
            n = read(cliSocket, buffer, 5);
            if (n <= 0) break;
            buffer[n] = '\0';
            tamano = atoi(buffer);

            n = read(cliSocket, buffer, tamano);
            if (n <= 0) break;
            buffer[n] = '\0';

            char nombre_p[101];
            n = read(cliSocket, nombre_p, 5);
            if (n <= 0) break;
            nombre_p[n] = '\0';
            tamano = atoi(nombre_p);

            n = read(cliSocket, nombre_p, tamano);
            if (n <= 0) break;
            nombre_p[n] = '\0';

            printf("\nMensaje de <%s>: %s\n", nombre_p, buffer);
         
        }
        else if (buffer[0] == 'f') {
            
            
            n = read(cliSocket, buffer, 5);
            buffer[n] = '\0';
            int len_dest = atoi(buffer);

            
            char dummy_dest[101];
            n = read(cliSocket, dummy_dest, len_dest);
            dummy_dest[n] = '\0';

        
            n = read(cliSocket, buffer, 100);
            buffer[n] = '\0';
            int len_nombre = atoi(buffer);

        
            char nombre_archivo[101];
            n = read(cliSocket, nombre_archivo, len_nombre);
            nombre_archivo[n] = '\0';

            
            char buffer_size[19];
            n = read(cliSocket, buffer_size, 18);
            buffer_size[n] = '\0';
            long file_size = atol(buffer_size);

            
            char* contenido = (char*)malloc(file_size);
            long remaining = file_size;
            long offset1 = 0;

            while (remaining > 0) {
                int chunk_size = remaining > 300 ? 300 : remaining;
                char temp_buffer[300]; // buffer temporal

                int n = read(cliSocket, temp_buffer, chunk_size);
                printf("El número es: %d\n", n);  // n es int, no hace falta %ld

                if (n <= 0) {
                    printf("Error en recepción: n=%d\n", n);
                    break;
                }

                memcpy(contenido + offset1, temp_buffer, n);  // copiamos al buffer final
                offset1+= n;
                remaining -= n;
                }

            char nuevo_nombre[256];


            char* punto = strrchr(nombre_archivo, '.');

            if (punto) {
               
                size_t len_base = punto - nombre_archivo;
                strncpy(nuevo_nombre, nombre_archivo, len_base);
                nuevo_nombre[len_base] = '\0';  

                strcat(nuevo_nombre, "copia");
                strcat(nuevo_nombre, punto);   
            } else {
                
                sprintf(nuevo_nombre, "%scopia", nombre_archivo);
}
           
            FILE* f = fopen(nuevo_nombre, "wb");
            if (!f) {
                perror("No se pudo crear el archivo");
                free(contenido);
                return NULL;
            }
            fwrite(contenido, 1, file_size, f);
            fclose(f);
            free(contenido);

            printf("\nArchivo recibido de <%s> y guardado como '%s' (%ld bytes).\n",
                dummy_dest, nombre_archivo, file_size);
        }
        else if(buffer[0]=='T'){
            n = read(cliSocket, buffer, 1);
            michi=buffer[0];
            printf("\nTu Turno %c \n",michi);
        }
        else if(buffer[0]=='E'){
            char recibe[101];
            n=read(cliSocket,recibe,1);
            recibe[n]='\0';
            int nerror=atoi(recibe);
            n=read(cliSocket,recibe,5);
            recibe[n]='\0';
            int tamano = atoi(recibe);
            n=read(cliSocket,recibe,tamano);
            recibe[n]='\0';

            printf("\nError <%01d> %s\n",nerror,recibe);
        }
        else if(buffer[0]=='X'){
            char recibemichi[15];
            n=read(cliSocket,recibemichi,9);
            recibemichi[n]='\0';
            printf("\n");
            for (int i = 0; i < 9; i++) {
                printf(" %c ", recibemichi[i]);
                if (i % 3 != 2) printf("|");
                if (i % 3 == 2 && i != 8) printf("\n---+---+---\n");
            }
            printf("\n");

        }
        else if(buffer[0]=='O'){
            //printf("\nsi llegue 4\n");
            char  l[4];
            n=read(cliSocket,l,1);

            l[1] = '\0';  
            if (n <= 0) {
                perror("Error al leer el resultado o conexión cerrada");
            } else {
            printf("\nRESULTADO DEL JUEGO : %c",l[0]);
            fflush(stdout);}
        }

    }

    shutdown(cliSocket, SHUT_RDWR);
    close(cliSocket);
    return NULL;
}
void list_users(int soketfd){
    char buffer_temp[8]="00001L\0";
    write(soketfd,buffer_temp,6);
    return;
}
void chatear(int soketfd){;
    char buffer[101];
    char buffer2[201];
    char buffer3[317];

    printf("%s : ", para);
        fflush(stdout);
        fgets(buffer, 100, stdin);
        int tamano1 = strlen(buffer);
        buffer[tamano1 - 1] = '\0'; 
        //printf("%d\n", n);
       
        printf("%s : ", msgg);
        fflush(stdout);
        fgets(buffer2, 200, stdin);
        int tamano2= strlen(buffer2);
        buffer2[tamano2 - 1] = '\0';
        //aqui el tamano es tamano-1 porque en el la cadena de char lo toma como \n el ultimo caracter;

        sprintf(buffer3,"%05dM%05d%s%05d%s",tamano1+tamano2-1,tamano2-1,buffer2,tamano1-1,buffer);
        printf("\n%s\n",buffer3);
        write(soketfd, buffer3, tamano1+tamano2+5+4+5);
        

    return;
}

 void quit(int soketfd){
    char buffer_temp[8]="00001Q\0";
   write(soketfd,buffer_temp,6);  
    std::cout << "Saliendo del chat...\n";


 }
    
void mssjgeneral(int soketfd){
    char buffer[101];
    
    char buffer3[301];

    printf("mensaje : ");
    fflush(stdout);
    fgets(buffer, 100, stdin);
    int tamano1 = strlen(buffer);
    buffer[tamano1 - 1] = '\0'; 
    sprintf(buffer3,"%05dB%05d%s",tamano1+5,tamano1-1,buffer);
    
    write(soketfd,buffer3,tamano1+5+5);
    printf("\n%s\n",buffer3);
    printf("\n%d\n",tamano1+5+5);

 }
void jugarmichi(int soketfd){
    char buffer_temp[8]="00001J\0";
   write(soketfd,buffer_temp,6);  
    std::cout << "SOLICITANDO\n";


 }
void enviarArchivo(int socketfd) {
    char buffer[101];    // Para destino
    char buffer2[201];   // Para nombre del archivo

    // Pedir DESTINO
    printf("Destino : ");
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    int tamanodestino = strlen(buffer);
    if (buffer[tamanodestino - 1] == '\n') buffer[tamanodestino - 1] = '\0';
    tamanodestino = strlen(buffer); // Actualizar tamaño real

    // Pedir NOMBRE DEL ARCHIVO
    printf("Archivo a enviar : ");
    fflush(stdout);
    fgets(buffer2, sizeof(buffer2), stdin);
    int tamanonamefile = strlen(buffer2);
    if (buffer2[tamanonamefile - 1] == '\n') buffer2[tamanonamefile - 1] = '\0';
    tamanonamefile = strlen(buffer2); // Actualizar tamaño real

    FILE* file = fopen(buffer2, "rb");
    if (!file) {
        perror("No se pudo abrir el archivo");
        return;
    }

    // Obtener tamaño del archivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Leer archivo en memoria
    char* file_data = (char*)malloc(file_size);
    if (!file_data) {
        perror("No se pudo reservar memoria para el archivo");
        fclose(file);
        return;
    }
    fread(file_data, 1, file_size, file);
    fclose(file);

    // Crear cabecera en string (sin binarios aún)
    char header[1024]; // tamaño suficiente para la cabecera
    int header_len = snprintf(header, sizeof(header), "%05ldF%05d%s%0100d%s%018ld",
                              0L, // Placeholder para tamaño total (lo pondremos luego)
                              tamanodestino, buffer,
                              tamanonamefile, buffer2,
                              file_size);

    if (header_len < 0 || header_len >= sizeof(header)) {
        fprintf(stderr, "Error al construir la cabecera\n");
        free(file_data);
        return;
    }
    
    // Calcular tamaño total real
    long total_size = header_len + file_size;
    //snprintf(header, 6, "%05ld", total_size); // Escribir tamaño real en los primeros 5 bytes
    printf("\n%s\n",header);
    printf("\n%d\n",header_len);
    // Construir el buffer final
    char* final_buffer = (char*)malloc(total_size);
    if (!final_buffer) {
        perror("No se pudo reservar memoria para el mensaje final");
        free(file_data);
        return;
    }

    memcpy(final_buffer, header, header_len);                // Copiar cabecera
    memcpy(final_buffer + header_len, file_data, file_size); // Copiar contenido binario
    for (int i = 0; i < header_len; i++) {
    printf("%c", final_buffer[i]);
}
printf("\n");
    // Enviar mensaje completo
    write(socketfd, final_buffer, total_size);

    // Liberar memoria
    free(file_data);
    free(final_buffer);
}

void enviarposs(int socketfd){
    int poss=0;
    printf("\nposiion: ");
    cin>>poss;
    std::cin.ignore(); 
    if ( poss<0 || poss>9){
        printf("\nposicion no valida\n");
        return;
    }
    char enviar[20];
    sprintf(enviar,"%05dP%01d%c",3,poss,michi);
    write(socketfd,enviar,8);

}

void viewmichi(int socketfd){
    char buffer_temp[8]="00001V\0";
   write(socketfd,buffer_temp,6);  
    //std::cout << "Saliendo del chat...\n";
}
int main(void) {
    struct sockaddr_in stSockAddr;
    int Res;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    int n;
    char buffer[250];

    if (-1 == SocketFD) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(1100);
    Res = inet_pton(AF_INET, "192.168.1.100", &stSockAddr.sin_addr);

    if (0 > Res) {
        perror("error: first parameter is not a valid address family");
        close(SocketFD);
        exit(EXIT_FAILURE);
    } else if (0 == Res) {
        perror("char string (second parameter does not contain valid ipaddress");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in))) {
        perror("connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    char nombre[150];
    pthread_t thread;
    
    
    printf("nombre: ");
    fflush(stdout);
    
    fgets(nombre, 100, stdin);
    int tamano = strlen(nombre);
    nombre[tamano - 1] = '\0'; 
    tamano = strlen(nombre);
    sprintf(buffer,"%05dN%s",tamano+1,nombre);
    
    //cout<<tamano<<'\n';
    write(SocketFD,buffer, tamano+1+5);
    pthread_create(&thread, NULL, readSocketThread, &SocketFD);
    pthread_detach(thread);


        while (true) {
        int opcion=0;    
        std::cout << "\n--- MENÚ ---\n";
        std::cout << "1. Mostrar lista de usuarios\n";
        std::cout << "2. Enviar mensaje a otro usuario\n";
        std::cout << "3. Broadcast\n";
        std::cout << "4. Enviar archivo \n";
        std::cout<<  "5. Unirse a la partida #\n";
        std::cout<<  "6. Enviar posicion #\n";
        std::cout<<  "7. Ver partida #\n";
        std::cout << "8. Salir\n";
        
        std::cout << "Seleccione una opción: ";
        std::cin >> opcion;
        std::cin.ignore();  

        if (opcion == 1) {
            list_users(SocketFD);
        }
        else if (opcion == 2) {
           chatear(SocketFD);
        }
        else if (opcion == 3) {
            
            mssjgeneral(SocketFD);
        }
        else if (opcion == 8) {
            quit(SocketFD);
            break;
        }
        else if (opcion == 7) {
            viewmichi(SocketFD);
        }
        else if (opcion == 4) {
            enviarArchivo(SocketFD);
        }
        else if (opcion == 5) {
           jugarmichi(SocketFD);
        }
        else if (opcion == 6) {
           enviarposs(SocketFD);
        }

        else {
            std::cout << "Opción inválida. Intenta de nuevo.\n";
        }
    }

    /*do {
        controlmsg = 1;
        printf("%s : ", para);
        fflush(stdout);
        fgets(buffer, 100, stdin);
        n = strlen(buffer);
        buffer[n - 1] = '\0'; // Eliminar el newline
        printf("%d\n", n);
        write(SocketFD, buffer, n-1);
        
        controlmsg = 0;
        printf("%s : ", msgg);
        fflush(stdout);
        fgets(buffer, 100, stdin);
        n = strlen(buffer);
        buffer[n - 1] = '\0'; // Eliminar el newline
        write(SocketFD, buffer, n-1);

    } while (strncmp(buffer, "exit", 4) != 0);
    */

    shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);
    return 0;
}

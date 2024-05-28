#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>


//Struktura polaczenia z serwerem (connected to flaga czy poloczony)
struct connection {
    int sockfd;
    struct sockaddr_in server_addr;
    int connected;
    char namebuffer[100];
};

char* send(char *buffer, int sockfd) {
    send(sockfd, buffer, strlen(buffer), 0);
    return buffer;
}

int main(int argc, char *argv[]) {
    //sprawdzenie czy podano odpowiednia ilosc argumentow
    if (argc < 3) {
        printf("Wywolanie: %s <adres IP> <port> <nazwa>\n", argv[0]);
        exit(1);
    }

    //inicjalizacja struktury polaczenia
    struct connection conn;
    //zerowanie struktury
    conn.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn.sockfd == -1) {
        perror("socket");
        exit(1);
    }
    bzero((char *)&conn.server_addr, sizeof(conn.server_addr));
    bzero((char *)&conn.namebuffer, sizeof(conn.namebuffer));

    //sprawdzamy czy podany adres IP jest poprawny
    if (inet_pton(AF_INET, argv[1], &conn.server_addr.sin_addr) <= 0) {
        printf("inet_pton() ERROR: Invalid address\n");
        exit(101);
    } 

    //ustawienie rodziny protokolow
    conn.server_addr.sin_family = AF_INET;
    //ustawienie portu
    conn.server_addr.sin_port = htons(atoi(argv[2]));

    //polaczenie z serwerem
    if (connect(conn.sockfd, (struct sockaddr *)&conn.server_addr, sizeof(conn.server_addr)) < 0) {
        perror("connect() ERROR");
        exit(3);
    }

    while(1){

        sleep(1);
    
        //wyslanie nazwy do serwera
        strcpy(conn.namebuffer, argv[3]);
        send(conn.sockfd, conn.namebuffer, strlen(conn.namebuffer), 0);

        //wyslanie wiadomosci "hello" do serwera
        char buffer[200];
        strcpy(buffer, "hello");
        send(conn.sockfd, buffer, strlen(buffer), 0);

        //odbior wiadomosci od serwera
        recv(conn.sockfd, buffer, sizeof(buffer), 0);
        printf("Server: %s\n", buffer);
    }

    //zamkniecie polaczenia
    close(conn.sockfd);
    return 0;
}


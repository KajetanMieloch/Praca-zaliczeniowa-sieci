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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Wywolanie: %s <adres IP> <port> <nazwa>\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in server;
    char namebuffer[100];

    bzero((char *)&server, sizeof(server));
    bzero((char *)&namebuffer, sizeof(namebuffer));

    if (inet_pton(AF_INET, argv[1], &server.sin_addr) == 0) {
        printf("inet_pton() ERROR: Invalid address\n");
        exit(101);
    } else if (inet_pton(AF_INET, argv[1], &server.sin_addr) < 0) {
        perror("inet_pton() ERROR");
        exit(101);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket() ERROR");
        exit(2);
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect() ERROR");
        exit(3);
    }

    strcpy(namebuffer, argv[3]);
    send(sock, namebuffer, strlen(namebuffer), 0);

    char buffer[200];

    //send message "hello" to server on connection
    strcpy(buffer, " A");
    send(sock, buffer, strlen(buffer), 0);

    // //receive message from server
    // recv(sock, buffer, sizeof(buffer), 0);
    // printf("Server: %s\n", buffer);


    printf("\nKoniec.\n");
    fflush(stdout);
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return 0;
}
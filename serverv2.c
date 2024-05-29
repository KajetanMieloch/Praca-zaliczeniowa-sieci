#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>

#define SERWER_IP "127.0.0.1"
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

struct connection {
    int sock;
    int send;
    int port;
    char *name;
    struct sockaddr_in server;
    int connected;
    int error;
    char status;
};

int main(int argc, char *argv[]) {

    //lista polaczen
    struct connection connections[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connections[i].connected = 0;
        connections[i].error = 0;
        connections[i].status = ' ';
    }

    if (argc < 2) {
        printf("Wywolanie: %s <port>\n", argv[0]);
        exit(1);
    }

    int SERWER_PORT = atoi(argv[1]);

    //inicjalizacja serwera z selectem dla max_clients
    struct sockaddr_in serwer = {
        .sin_family = AF_INET,
        .sin_port = htons(SERWER_PORT),
    };

    //tworzenie gniazda
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    /////Obsluga bledow/////
    //sprawdzamy czy podany adres IP jest poprawny
    if (inet_pton(AF_INET, SERWER_IP, &serwer.sin_addr) <= 0) {
        perror("inet_pton() ERROR");
        exit(1);
    }
    //sprawdzamy czy gniazdo zostalo poprawnie utworzone
    if (sockfd < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    //sprawdzamy czy gniazdo zostalo poprawnie przypisane do portu
    if (bind(sockfd, (struct sockaddr *)&serwer, sizeof(serwer)) < 0) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }
    //sprawdzamy czy gniazdo jest gotowe do nasluchiwania
    if (listen(sockfd, 5) < 0) {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", SERWER_PORT);

    //inicjalizacja selecta
    fd_set active_fds;
    FD_ZERO(&active_fds);
    FD_SET(sockfd, &active_fds);

    int max_fd = sockfd;

    while (1) {
        fd_set read_fds = active_fds;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select() failed");
            exit(EXIT_FAILURE);
        }

        //sprawdzanie czy jakies gniazdo jest gotowe do odczytu
        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                //Jeżeli gniazdo to gniazdo serwera to znaczy, że jest nowe połączenie
                if (i == sockfd) {
                    // Accept new client
                    struct sockaddr_in clientaddr;
                    socklen_t clientlen = sizeof(clientaddr);

                    int client_sock = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
                    if (client_sock < 0) {
                        perror("accept() failed");
                        exit(EXIT_FAILURE);
                    }

                    //dodanie nowego klienta do listy polaczen
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (connections[j].connected == 0) {
                            connections[j].sock = client_sock;
                            connections[j].connected = 1;
                            connections[j].server = clientaddr;
                            connections[j].port = ntohs(clientaddr.sin_port);
                            break;
                        }
                    }

                    //dodanie nowego klienta do selecta
                    FD_SET(client_sock, &active_fds);
                    if (client_sock > max_fd) {
                        max_fd = client_sock;
                    }
                } else {
                    // Odczytanie wiadomosci od klienta
                    char buffer[BUFFER_SIZE];
                    int read = recv(i, buffer, BUFFER_SIZE, 0);

                    //Wiadomosc od klienta
                    printf("Received: %s\n", buffer);
                    
                    if (read < 0) {
                        perror("recv() failed");
                        exit(EXIT_FAILURE);
                    } else if (read == 0) {
                        // Klient sie rozlaczyl
                        close(i);
                        FD_CLR(i, &active_fds);
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (connections[j].sock == i) {
                                connections[j].connected = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

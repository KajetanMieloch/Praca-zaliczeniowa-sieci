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
#define DATA_SIZE 1000 // 250 values, 4 bytes each
#define TOTAL_VALUES 100000 // 400 packets
#define HISTOGRAM_SIZE 16

#define MAX_DATA 100000
struct CALCDATA{
uint32_t data[MAX_DATA];
};

struct connection {
    int sock;
    int send;
    int port;
    char *name;
    struct sockaddr_in server;
    int connected;
    int error;
    char status;
    int data_index;
    struct CALCDATA cdata;
};

// Dane do obliczeń: są tworzone przez serwer, mają format uint32_t i zakres 0-65535. To 
// znaczy że każda wartość & 0xffff0000 == 0. Każdy pakiet danych będzie miał wielkość 
// dokładnie 100000 wartości. Funkcja generująca dane zostanie podana w osobnym 
// dokumencie. Należy ją włączyć do kodu i wywołać przy każdym tworzeniu nowego 
// zestawy danych.


int create_data(int idx, struct CALCDATA *cdata)
{
    if (cdata != NULL){
    uint32_t i;
    uint32_t *value;
    uint32_t v;
    value = &cdata->data[0];
        for (i=0; i<MAX_DATA; i++){
            v = (uint32_t) rand() ^ (uint32_t) rand();
            printf("Creating value #%d v=%u addr=%lu \r",i,v,(unsigned long)value);
            *value = v & 0x0000FFFF;
            value++;
        }
    return(1);
    }
return(0);
}

char* recivemessage(char* message, struct connection *connection, int udp_sock) {
    static char response[1000];
    bzero(response, 1000);
    response[0] = '@';
    char command = message[11];
    printf("Received command: %c\n", command); // Debug print
    switch (command) {
        case 'N': {
            printf("%i",connection->server.sin_port);
            int data_port = connection->port + 1; // Calculate data port
            struct sockaddr_in data_addr = connection->server;
            data_addr.sin_port = htons(data_port);
            bind(udp_sock, (struct sockaddr *)&data_addr, sizeof(data_addr));
            // Send response with data port
            sprintf(response, "@000000000!P:%d#", data_port);
            printf("Sending response: %s\n", response); // Debug print
            break;
        }
        case 'D':
            // Handle other commands as necessary
            sprintf(response, "@000000000!X:0#");
            printf("Sending response: %s\n", response); // Debug print
            break;
        default:
            printf("Unknown command: %c\n", command);
            break;
    }
    return response;
}

int main(int argc, char *argv[]) {

    int SERWER_PORT = atoi(argv[1]);
    // Lista połączeń
    struct connection connections[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        connections[i].connected = 0;
        connections[i].error = 0;
        connections[i].status = ' ';
        connections[i].port = SERWER_PORT + 1 + i;
        connections[i].data_index = i;
        create_data(i, &connections[i].cdata);

    }

    if (argc < 2) {
        printf("Wywolanie: %s <port>\n", argv[0]);
        exit(1);
    }

    // Inicjalizacja serwera z selectem dla max_clients
    struct sockaddr_in serwer = {
        .sin_family = AF_INET,
        .sin_port = htons(SERWER_PORT),
    };

    // Tworzenie gniazda
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Obsluga bledow
    if (inet_pton(AF_INET, SERWER_IP, &serwer.sin_addr) <= 0) {
        perror("inet_pton() ERROR");
        exit(1);
    }
    if (sockfd < 0 || udp_sock < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    if (bind(sockfd, (struct sockaddr *)&serwer, sizeof(serwer)) < 0) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }
    if (listen(sockfd, 5) < 0) {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", SERWER_PORT);

    // Inicjalizacja selecta
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

        // Sprawdzanie czy jakies gniazdo jest gotowe do odczytu
        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                // Nowe połączenie
                if (i == sockfd) {
                    struct sockaddr_in clientaddr;
                    socklen_t clientlen = sizeof(clientaddr);
                    int client_sock = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);

                    int port = SERWER_PORT + 1 + connections[0].data_index;

                    if (client_sock < 0) {
                        perror("accept() failed");
                        exit(EXIT_FAILURE);
                    }
                    printf("Accepted new connection on socket %d\n", client_sock); // Debug print
                    // Dodanie nowego klienta do listy polaczen
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (connections[j].connected == 0) {
                            connections[j].sock = client_sock;
                            connections[j].connected = 1;
                            connections[j].server = clientaddr;
                            connections[j].port = port;
                            break;
                        }
                    }
                    // Dodanie nowego klienta do selecta
                    FD_SET(client_sock, &active_fds);
                    if (client_sock > max_fd) {
                        max_fd = client_sock;
                    }
                } else {
                    // Odczytanie wiadomosci od klienta
                    char buffer[BUFFER_SIZE];
                    int read = recv(i, buffer, BUFFER_SIZE, 0);
                    if (read < 0) {
                        perror("recv() failed");
                        exit(EXIT_FAILURE);
                    } else if (read == 0) {
                        // Klient sie rozlaczyl
                        printf("Client on socket %d disconnected\n", i); // Debug print
                        close(i);
                        FD_CLR(i, &active_fds);
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (connections[j].sock == i) {
                                connections[j].connected = 0;
                                break;
                            }
                        }
                    } else {
                        // Wiadomosc od klienta
                        buffer[read] = '\0';
                        printf("Received message from client: %s\n", buffer); // Debug print
                        char* response = recivemessage(buffer, &connections[i], udp_sock);
                        send(i, response, strlen(response), 0);
                        printf("Sent response to client: %s\n", response); // Debug print
                    }
                }
            }
        }
    }
}

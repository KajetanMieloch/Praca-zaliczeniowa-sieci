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

uint32_t bitcnt[HISTOGRAM_SIZE];

void update_histogram(uint32_t *data) {
    for (int i = 0; i < DATA_SIZE / sizeof(uint32_t); i++) {
        uint32_t value = data[i];
        if ((value & 0xFFFF0000) == 0) {
            for (int bit = 0; bit < 16; bit++) {
                if (value & (1 << bit)) {
                    bitcnt[bit]++;
                }
            }
        }
    }
}

void handle_data_connection(int udp_sock, struct sockaddr_in clientaddr) {
    uint32_t data[DATA_SIZE / sizeof(uint32_t) + 1]; // Additional uint32_t for counter
    socklen_t clientlen = sizeof(clientaddr);
    for (int i = 0; i < TOTAL_VALUES / (DATA_SIZE / sizeof(uint32_t)); i++) {
        data[0] = i | 0x55550000; // Counter with 0x55550000 mask
        for (int j = 1; j < DATA_SIZE / sizeof(uint32_t) + 1; j++) {
            data[j] = rand() & 0xFFFF; // Random values with lower 16 bits
        }
        sendto(udp_sock, data, sizeof(data), 0, (struct sockaddr *)&clientaddr, clientlen);
        // Wait for acknowledgment
        char ack[1];
        recvfrom(udp_sock, ack, sizeof(ack), 0, (struct sockaddr *)&clientaddr, &clientlen);
        // Update histogram
        update_histogram(&data[1]);
    }
}

char* recivemessage(char* message, struct connection *connection, int udp_sock) {
    static char response[1000];
    bzero(response, 1000);
    response[0] = '@';
    char command = message[11];
    printf("Received command: %c\n", command); // Debug print
    switch (command) {
        case 'N': {
            int data_port = connection->port + 1; // Calculate data port
            struct sockaddr_in data_addr = connection->server;
            data_addr.sin_port = htons(data_port);
            bind(udp_sock, (struct sockaddr *)&data_addr, sizeof(data_addr));
            // Send response with data port
            sprintf(response, "@000000000!P:%d#", data_port);
            printf("Sending response: %s\n", response); // Debug print
            handle_data_connection(udp_sock, connection->server);
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
    // Lista połączeń
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
                            connections[j].port = ntohs(clientaddr.sin_port);
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

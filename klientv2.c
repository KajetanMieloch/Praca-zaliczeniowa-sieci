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

#define BUFFER_SIZE 4096
#define DATA_SIZE 1000 // 250 values, 4 bytes each
#define TOTAL_VALUES 100000 // 400 packets

// Struktura polaczenia z serwerem (connected to flaga czy poloczony)
struct connection {
    int sockfd;
    struct sockaddr_in server_addr;
    int connected;
    char namebuffer[100];
};

char* sendmessage(char* client_name, char option) {
    static char message[1000];
    // Zerowanie bufora
    bzero(message, 1000);
    message[0] = '@';

    switch (option) {
        case 'N':
            // Skopiowanie maksymalnie 8 znaków z client_name do message[1]
            strncpy(&message[1], client_name, 8);

            // Wypełnienie pozostałych miejsc od 1 do 8 zerami, jeśli nazwa jest krótsza niż 8 znaków
            for (int i = 1; i <= 8; i++) {
                if (message[i] == '\0') {
                    message[i] = '0';
                }
            }
            message[9] = '0';
            message[10] = '!';
            message[11] = 'N';
            message[12] = ':';
            for (int i = 13; i < 999; i++) {
                message[i] = '0';
            }
            message[999] = '#';

            break;
        default:
            message[1] = 'E';
            break;
    }

    printf("Wysylam: %s\n", message);
    return message;
}

void send_result(int sockfd, char* client_name, char option, int port) {
    char message[1000];
    bzero(message, 1000);
    message[0] = '@';

    // Skopiowanie maksymalnie 8 znaków z client_name do message[1]
    strncpy(&message[1], client_name, 8);

    // Wypełnienie pozostałych miejsc od 1 do 8 zerami, jeśli nazwa jest krótsza niż 8 znaków
    for (int i = 1; i <= 8; i++) {
        if (message[i] == '\0') {
            message[i] = '0';
        }
    }
    message[9] = '0';
    message[10] = '!';
    message[11] = option;
    message[12] = ':';
    snprintf(&message[13], 5, "%d", port);
    message[999] = '#';

    printf("Wysylam: %s\n", message);
    send(sockfd, message, 1000, 0);
}

void handle_data_connection(int udp_sock, struct sockaddr_in data_addr, char* client_name, int server_sock) {
    uint32_t data[DATA_SIZE / sizeof(uint32_t) + 1]; // Additional uint32_t for counter
    uint32_t bitcnt[16] = {0};
    socklen_t addr_len = sizeof(data_addr);
    char ack[1] = {1};

    for (int i = 0; i < TOTAL_VALUES / (DATA_SIZE / sizeof(uint32_t)); i++) {
        int received = recvfrom(udp_sock, data, sizeof(data), 0, (struct sockaddr *)&data_addr, &addr_len);
        if (received < 0) {
            perror("recvfrom() failed");
            send_result(server_sock, client_name, 'E', data_addr.sin_port);
            return;
        }

        // Process data and update histogram
        for (int j = 1; j < DATA_SIZE / sizeof(uint32_t) + 1; j++) {
            uint32_t value = data[j];
            if ((value & 0xFFFF0000) == 0) {
                for (int bit = 0; bit < 16; bit++) {
                    if (value & (1 << bit)) {
                        bitcnt[bit]++;
                    }
                }
            }
        }

        // Send acknowledgment
        sendto(udp_sock, ack, sizeof(ack), 0, (struct sockaddr *)&data_addr, addr_len);
    }

    // Send results back to the server
    send_result(server_sock, client_name, 'R', data_addr.sin_port);
}

int main(int argc, char *argv[]) {
    // Sprawdzenie czy podano odpowiednia ilosc argumentow
    if (argc < 4) {
        printf("Wywolanie: %s <adres IP> <port> <nazwa>\n", argv[0]);
        exit(1);
    }

    // Inicjalizacja struktury polaczenia
    struct connection conn;
    // Zerowanie struktury
    conn.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn.sockfd == -1) {
        perror("socket");
        exit(1);
    }
    bzero((char *)&conn.server_addr, sizeof(conn.server_addr));
    bzero((char *)&conn.namebuffer, sizeof(conn.namebuffer));

    // Skopiowanie nazwy klienta do struktury
    strncpy(conn.namebuffer, argv[3], 8);

    // Sprawdzamy czy podany adres IP jest poprawny
    if (inet_pton(AF_INET, argv[1], &conn.server_addr.sin_addr) <= 0) {
        printf("inet_pton() ERROR: Invalid address\n");
        exit(101);
    }

    // Ustawienie rodziny protokolow
    conn.server_addr.sin_family = AF_INET;
    // Ustawienie portu
    conn.server_addr.sin_port = htons(atoi(argv[2]));

    // Polaczenie z serwerem
    if (connect(conn.sockfd, (struct sockaddr *)&conn.server_addr, sizeof(conn.server_addr)) < 0) {
        perror("connect() ERROR");
        exit(3);
    } else {
        printf("Polaczono z serwerem\n");
        conn.connected = 1;
    }

    while (1) {
        sleep(1);

        // A1
        send(conn.sockfd, sendmessage(conn.namebuffer, 'N'), 1000, 0);
        printf("Wyslano zapytanie o port danych\n");
        // Odbieranie odpowiedzi od serwera
        char buffer[BUFFER_SIZE];
        int read = recv(conn.sockfd, buffer, BUFFER_SIZE, 0);
        if (read < 0) {
            perror("recv() failed");
            exit(EXIT_FAILURE);
        } else if (read == 0) {
            printf("Server disconnected\n");
            break;
        }

        buffer[read] = '\0';
        printf("Odebrano od serwera: %s\n", buffer);

        // Sprawdzenie komendy P
        if (buffer[11] == 'P') {
            // Wyodrebnienie numeru portu
            int data_port;
            sscanf(&buffer[13], "%d", &data_port);
            printf("Odebrano port danych: %d\n", data_port);

            // Ustawienie adresu serwera danych
            struct sockaddr_in data_addr;
            bzero((char *)&data_addr, sizeof(data_addr));
            data_addr.sin_family = AF_INET;
            data_addr.sin_port = htons(data_port);
            inet_pton(AF_INET, argv[1], &data_addr.sin_addr);

            // Tworzenie gniazda UDP do odbioru danych
            int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (udp_sock < 0) {
                perror("socket() failed");
                exit(EXIT_FAILURE);
            }
            handle_data_connection(udp_sock, data_addr, conn.namebuffer, conn.sockfd);
            close(udp_sock);
        }
    }

    // Zamkniecie polaczenia
    printf("Zamykanie polaczenia\n");
    conn.connected = 0;
    close(conn.sockfd);
    return 0;
}

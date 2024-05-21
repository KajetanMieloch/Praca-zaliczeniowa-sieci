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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Wywolanie: %s <port>\n", argv[0]);
        exit(1);
    }

    int SERWER_PORT = atoi(argv[1]);

    struct sockaddr_in serwer = {
        .sin_family = AF_INET,
        .sin_port = htons(SERWER_PORT),
    };

    if (inet_pton(AF_INET, SERWER_IP, &serwer.sin_addr) <= 0) {
        perror("inet_pton() ERROR");
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
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

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sockfd) {
                    // Accept new client
                    struct sockaddr_in clientaddr;
                    socklen_t clientlen = sizeof(clientaddr);

                    int client_sock = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
                    if (client_sock < 0) {
                        perror("accept() failed");
                        exit(EXIT_FAILURE);
                    }

                    printf("Accepted connection from %s:%d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

                    FD_SET(client_sock, &active_fds);
                    if (client_sock > max_fd) {
                        max_fd = client_sock;
                    }
                } else {
                    // Handle data from a client
                    char buffer[BUFFER_SIZE];
                    int bytes = recv(i, buffer, sizeof(buffer), 0);
                    if (bytes < 0) {
                        perror("recv() failed");
                        exit(EXIT_FAILURE);
                    } else if (bytes == 0) {
                        printf("Client disconnected\n");
                        close(i);
                        FD_CLR(i, &active_fds);
                    } else {
                        buffer[bytes] = '\0';
                        printf("Received: %s\n", buffer);
                }
                // //Now send "B" to client
                // char buffer2[200];
                // strcpy(buffer2, "B");
                // send(i, buffer2, strlen(buffer2), 0);
            }
        }
            // // Send a message to all connected clients
            // const char message[] = "Hello, client!\n";
            // for (int i = 0; i <= max_fd; i++) {
            //     if (i != sockfd && FD_ISSET(i, &active_fds)) {
            //         if (send(i, message, strlen(message), 0) < 0) {
            //             perror("send() ERROR");
            //             exit(EXIT_FAILURE);
            //         }
            //     }
            // }
    }

    return 0;
}}
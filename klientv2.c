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

char* sendmessage(char* client_name, char option) {
 
    static char message[1000];
    //zerowanie bufora
    bzero(message, 1000);
    message[0] = '@';

    switch(option) {
        case 'N': 
            // Skopiowanie maksymalnie 8 znaków z client_name do message[1]
            strncpy(&message[1], client_name, 8);

            // Wypełnienie pozostałych miejsc od 1 do 8 zerami, jeśli nazwa jest krótsza niż 8 znaków
            for(int i = 1; i <= 8; i++) {
                if (message[i] == '\0') {
                    message[i] = '0';
                }
            }
            message[9] = '0';
            message[10] = '!';
            message[11] = 'N';
            message[12] = ':';
            for(int i=13;i<999;i++){
                message[i]='0';
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

    //skopiowanie nazwy klienta do struktury
    strncpy(conn.namebuffer, argv[3], 8);

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
    } else {
        printf("Polaczono z serwerem\n");
        conn.connected = 1;
    }

    while(1){

        sleep(1);
    
        //A1
        send(conn.sockfd, sendmessage(conn.namebuffer, 'N'), 1000, 0);
    }

    //zamkniecie polaczenia
    printf("Zamykanie polaczenia\n");
    conn.connected = 0;
    close(conn.sockfd);
    return 0;
}


#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

#define BUFSZ 1024

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("exemple: %s 127.0.0.1 5151\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != addrparse(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int clientSocket;
    clientSocket = socket(storage.ss_family, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        logexit("socket");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != connect(clientSocket, addr, sizeof(storage))) {
        logexit("connect");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    printf("connected to %s \n", addrstr);

    char message[BUFSZ];
    char response[BUFSZ];

    while (1) {
        fflush(stdin);
        memset(message, 0, BUFSZ);
        memset(response, 0, BUFSZ);

        fgets(message, BUFSZ, stdin);

        int sentMessageLength = send(clientSocket, message, strlen(message) + 1, 0);
        if (sentMessageLength != strlen(message) + 1) {
            logexit("send");
        }

        int responseMessageLength = recv(clientSocket, response, BUFSZ, 0);
        if (responseMessageLength < 1) {
            exit(-1);
        }

        printf("%s\n", response);
    }

    close(clientSocket);

    exit(EXIT_SUCCESS);
}

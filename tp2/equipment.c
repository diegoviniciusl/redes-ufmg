#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

#define BUFSZ 1024
#define MAX_EQUIPMENTS 10

#define REQ_RM_ID 2
#define RES_ADD_ID 3
#define RES_LIST_ID 4
#define REQ_INF_ID 5
#define RES_INF_ID 6
#define ERROR_ID 7
#define OK_ID 8

// ERRO_ID SUB TYPES
#define EQUIPMENT_NOT_FOUND 1
#define SOURCE_EQUIPMENT_NOT_FOUND 2
#define TARGET_EQUIPMENT_NOT_FOUND 3
#define EQUIPMENT_LIMIT_EXCEEDED 4

typedef struct
{
    char *id;
    int socket;
} Equipment;

Equipment equipment = {.id = "", .socket = -1};

pthread_t tidReceiver;
pthread_t tidSender;

int keepConnectionAlive = 1;

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 5151\n", argv[0]);
    exit(EXIT_FAILURE);
}

void sendMessage(char message[]) {
    int sentMessageLength = send(equipment.socket, message, strlen(message), 0);
    if (sentMessageLength != strlen(message)) {
        logexit("send");
    }
}

void resInfoHandler() {
    char *equipmentId = strtok(NULL, "-");
    char *temperature = strtok(NULL, "-");

    printf("Value from %s: %s\n", equipmentId, temperature);
}

void reqInfoHandler() {
    char *equipmentId = strtok(NULL, "-");
    int number = rand() % 10;
    int decimal = (rand() % 89) + 10;

    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%i-%i.%i", RES_INF_ID, atoi(equipmentId), number, decimal);

    printf("requested information\n");

    sendMessage(message);
}

void resListHandler() {
    char *equipmentIds = strtok(NULL, "-");

    if (equipmentIds != NULL) {
        printf("%s\n", equipmentIds);
    } else {
        printf("\n");
    }
}

void reqRemoveHandler() {
    char *equipmentId = strtok(NULL, "-");
    printf("Equipment %s removed\n", equipmentId);
}

void resAddHandler() {
    char *equipmentId = strtok(NULL, "-");

    if (strcmp(equipment.id, "") == 0) {
        equipment.id = equipmentId;
        printf("New ID: %s\n", equipment.id);
        return;
    }

    printf("Equipment %s added\n", equipmentId);
}

void errorHandler() {
    int errorType = atoi(strtok(NULL, "-"));

    switch (errorType) {
        case EQUIPMENT_NOT_FOUND:
            printf("Equipment not found\n");
            break;
        case SOURCE_EQUIPMENT_NOT_FOUND:
            printf("Source equipment not found\n");
            break;
        case TARGET_EQUIPMENT_NOT_FOUND:
            printf("Target equipment not found\n");
            break;
        case EQUIPMENT_LIMIT_EXCEEDED:
            printf("Equipment limit exceeded\n");
            keepConnectionAlive = 0;
            break;
        default:
            printf("Invalid error message\n");
    }
}

void okHandler() {
    printf("Success\n");
    keepConnectionAlive = 0;
}

void processMessage(char originalMessage[]) {
    char message[BUFSZ];
    strcpy(message, originalMessage);

    char *value = strtok(message, "-");
    int command = atoi(value);

    switch (command) {
        case RES_ADD_ID:
            resAddHandler();
            break;
        case REQ_RM_ID:
            reqRemoveHandler();
            break;
        case RES_LIST_ID:
            resListHandler();
            break;
        case REQ_INF_ID:
            reqInfoHandler();
            break;
        case RES_INF_ID:
            resInfoHandler();
            break;
        case ERROR_ID:
            errorHandler();
            break;
        case OK_ID:
            okHandler();
            break;
        default:
            printf("Invalid message\n");
    }
}

void *receiverThread(void *data) {
    while (keepConnectionAlive) {
        char message[BUFSZ];
        memset(message, 0, BUFSZ);

        int messageLength = recv(equipment.socket, message, BUFSZ, 0);

        if (messageLength < 1) {
            exit(-1);
        }

        processMessage(message);
    }

    pthread_exit(EXIT_SUCCESS);
}

void readClientInput(char input[], char message[]) {
    if (strcmp(input, "close connection\n") == 0) {
        sprintf(message, "%i", REQ_RM_ID);
    }
    if (strcmp(input, "list equipment\n") == 0) {
        sprintf(message, "%i", RES_LIST_ID);
    }
    char *value = strtok(input, " ");
    if (strcmp(value, "request") == 0) {
        value = strtok(NULL, " ");
        if (strcmp(value, "information") == 0) {
            value = strtok(NULL, " ");
            if (strcmp(value, "from") == 0) {
                value = strtok(NULL, " ");
                if (value != NULL) {
                    value[strcspn(value, "\n")] = 0;
                    sprintf(message, "%i-%s", REQ_INF_ID, value);
                }
            }
        }
    }
}

void *senderThread(void *data) {
    while (keepConnectionAlive) {
        char input[BUFSZ];
        memset(input, 0, BUFSZ);

        fgets(input, BUFSZ - 1, stdin);

        char message[BUFSZ];
        memset(message, 0, BUFSZ);

        readClientInput(input, message);

        if (strlen(message) == 0) {
            printf("Invalid message\n");
            continue;
        }

        sendMessage(message);
    }

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    srand(time(0));

    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != addrparse(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int clientSocket = socket(storage.ss_family, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        logexit("socket");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != connect(clientSocket, addr, sizeof(storage))) {
        logexit("connect");
    }

    fflush(stdin);

    equipment.socket = clientSocket;

    pthread_create(&tidReceiver, NULL, receiverThread, NULL);
    pthread_create(&tidSender, NULL, senderThread, NULL);

    while (keepConnectionAlive) {
        continue;
    }

    close(equipment.socket);
    exit(EXIT_SUCCESS);
}

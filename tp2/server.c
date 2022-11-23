#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
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
    char id[2];
    int socket;
    int connected;
} Equipment;

Equipment equipments[MAX_EQUIPMENTS] = {[0 ...(MAX_EQUIPMENTS - 1)] = {.id = "", .socket = -1, .connected = 0}};

void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 5151\n", argv[0]);
    exit(EXIT_FAILURE);
}

void unicastMessage(char *message, int socket) {
    int messageLength = send(socket, message, strlen(message) + 1, 0);
    if (messageLength != strlen(message) + 1) {
        logexit("send");
    }
}

void broadcastMessage(char *message) {
    for (int i = 0; i < MAX_EQUIPMENTS; ++i) {
        if (equipments[i].connected) {
            unicastMessage(message, equipments[i].socket);
        }
    }
}

void targetNotFoundErrorHandler(int id, char *value) {
    printf("Equipment %s not found\n", value);

    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%i", ERROR_ID, TARGET_EQUIPMENT_NOT_FOUND);
    unicastMessage(message, equipments[id].socket);
}

int getEquipmentIdByFormattedId(char *value) {
    for (int i = 0; i < MAX_EQUIPMENTS; ++i) {
        if (equipments[i].connected == 1 && strcmp(equipments[i].id, value) == 0) {
            return i;
        }
    }
    return -1;
}

void resInfoHandler(int id) {
    int equipmentId = atoi(strtok(NULL, "-"));
    char *temperature = strtok(NULL, "-");

    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%s-%s", RES_INF_ID, equipments[id].id, temperature);

    unicastMessage(message, equipments[equipmentId].socket);
}

void reqInfoHandler(int id) {
    char *value = strtok(NULL, "-");
    if (value == NULL) return;
    int equipmentId = getEquipmentIdByFormattedId(value);
    if (equipmentId == -1) {
        targetNotFoundErrorHandler(id, value);
        return;
    }
    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%i", REQ_INF_ID, id);
    unicastMessage(message, equipments[equipmentId].socket);
}

void resListHandler(int id) {
    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    char listEquipments[BUFSZ - 10] = "";
    for (int i = 0; i < MAX_EQUIPMENTS; ++i) {
        if (equipments[i].connected == 1 && i != id) {
            strcat(listEquipments, equipments[i].id);
            strcat(listEquipments, " ");
        }
    }
    sprintf(message, "%i-%s", RES_LIST_ID, listEquipments);
    unicastMessage(message, equipments[id].socket);
}

void removeEquipment(int id) {
    equipments[id].socket = -1;
    equipments[id].connected = 0;
    memset(equipments[id].id, 0, 2);
}

void reqRemoveHandler(int id) {
    printf("Equipment %s removed\n", equipments[id].id);
    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i", OK_ID);
    unicastMessage(message, equipments[id].socket);
    sleep(1);

    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%s", REQ_RM_ID, equipments[id].id);

    removeEquipment(id);
    broadcastMessage(message);
    sleep(1);
}

void limitExceededErrorHandler(int socket) {
    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%i", ERROR_ID, EQUIPMENT_LIMIT_EXCEEDED);
    unicastMessage(message, socket);
}

void resAddHandler(int id, int socket) {
    char message[BUFSZ];
    memset(message, 0, BUFSZ);
    sprintf(message, "%i-%s", RES_ADD_ID, equipments[id].id);
    broadcastMessage(message);
}

void normalizeEquipmentId(int id, char normalizedId[]) {
    id++;
    char charValue[3];
    sprintf(charValue, "%i", id);
    if (id < 10) {
        strcat(normalizedId, "0");
    }
    strcat(normalizedId, charValue);
}

int reqAddHandler(int socket) {
    int id = -1;
    for (int i = 0; i < MAX_EQUIPMENTS; ++i) {
        if (!equipments[i].connected) {
            id = i;
            break;
        }
    }
    if (id == -1) {
        limitExceededErrorHandler(socket);
    } else {
        char normalizedId[BUFSZ] = "";
        normalizeEquipmentId(id, normalizedId);
        equipments[id].id[0] = normalizedId[0];
        equipments[id].id[1] = normalizedId[1];
        equipments[id].connected = 1;
        equipments[id].socket = socket;
        printf("Equipment %s added\n", equipments[id].id);
        resAddHandler(id, socket);
    }
    return id;
}

int processMessage(int id, char originalMessage[]) {
    char message[BUFSZ];
    strcpy(message, originalMessage);

    char *value = strtok(message, "-");
    int command = atoi(value);

    switch (command) {
        case REQ_RM_ID:
            reqRemoveHandler(id);
            break;
        case RES_LIST_ID:
            resListHandler(id);
            break;
        case REQ_INF_ID:
            reqInfoHandler(id);
            break;
        case RES_INF_ID:
            resInfoHandler(id);
            break;
    }

    return 1;
}

void *clientThread(void *data) {
    int socket = *((int *)data);

    int id = reqAddHandler(socket);

    if (id == -1) {
        close(socket);
        pthread_exit(EXIT_SUCCESS);
    }

    char message[BUFSZ];
    while (equipments[id].connected) {
        memset(message, 0, BUFSZ);

        recv(equipments[id].socket, message, BUFSZ - 1, 0);

        int result = processMessage(id, message);
        if (result < 1) {
            close(equipments[id].socket);
            break;
        }
    }

    close(equipments[id].socket);
    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int serverSocket = socket(storage.ss_family, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(serverSocket, addr, sizeof(storage))) {
        logexit("bind");
    }

    if (0 != listen(serverSocket, 10)) {
        logexit("listen");
    }

    struct sockaddr_storage cstorage;
    struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
    socklen_t caddrlen = sizeof(cstorage);

    while (1) {
        int clientSocket = accept(serverSocket, caddr, &caddrlen);
        if (clientSocket == -1) {
            logexit("accept");
        }

        pthread_t tid;
        pthread_create(&tid, NULL, clientThread, &clientSocket);
    }

    exit(EXIT_SUCCESS);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

#define BUFSZ 1024
#define ADD 0
#define REMOVE 1
#define GET 2
#define LIST 3
#define INVALID -1
#define MAX_SWITCHES 3
#define MAX_RACKS 4

typedef struct {
    int id;
    int switches[MAX_SWITCHES];
} Rack;

int findRack(int id, Rack racks[]) {
    for (int i = 0; i < MAX_RACKS; i++) {
        if (racks[i].id == id) {
            return i;
        }
    }
    return -1;
}

int findFreeRack(Rack racks[]) {
    for (int i = 0; i < MAX_RACKS; i++) {
        if (racks[i].id == 0) {
            return i;
        }
    }
    return -1;
}

int isValidId(char *value) {
    return strcmp(value, "01") == 0 || strcmp(value, "02") == 0 || strcmp(value, "03") == 0 || strcmp(value, "04") == 0;
}

int getCommand(char command[]) {
    if (strcmp(command, "add") == 0) return ADD;
    if (strcmp(command, "rm") == 0) return REMOVE;
    if (strcmp(command, "get") == 0) return GET;
    if (strcmp(command, "ls") == 0) return LIST;
    return INVALID;
}

void normalizeId(int value, char normalizedValue[]) {
    char charValue[BUFSZ];
    sprintf(charValue, "%d", value);
    strcat(normalizedValue, "0");
    strcat(normalizedValue, charValue);
}

int addSwitches(Rack *rack, int switchesToBeAdded[], int switchCount, char response[]) {
    int installedSwitches = 0;
    strcat(response, "switch ");
    for (int i = 0; i < MAX_SWITCHES; i++) {
        if (rack->switches[i] == 0 && installedSwitches < switchCount) {
            rack->switches[i] = switchesToBeAdded[installedSwitches];
            char normalizedId[BUFSZ] = "";
            normalizeId(rack->switches[i], normalizedId);
            strcat(response, normalizedId);
            strcat(response, " ");
            installedSwitches++;
        }
    }
    strcat(response, "installed");
    return installedSwitches == switchCount;
}

int hasAvailableSwitchSpace(Rack *rack, int switchCount) {
    int availablesSwitches = 0;
    for (int i = 0; i < MAX_SWITCHES; i++) {
        if (rack->switches[i] == 0) {
            availablesSwitches += 1;
        }
    }
    return availablesSwitches >= switchCount;
}

int hasAlreadyInstalledSwitches(Rack *rack, int switchesToBeAdded[], int switchCount) {
    for (int i = 0; i < MAX_SWITCHES; i++) {
        for (int j = 0; j < switchCount; j++) {
            if (rack->switches[i] == switchesToBeAdded[j]) {
                return 1;
            }
        }
    }
    return 0;
}

int addHandler(char *value, Rack racks[], char response[]) {
    int part = 0;
    char *switchesRead[3] = {"0", "0", "0"};
    int switchCount = 0;
    char *rackIdRead = "0";
    int foundIn = 0;
    while (value != NULL) {
        value = strtok(NULL, " ");
        part += 1;

        // reading sw
        if (part == 1) {
            if (strcmp(value, "sw") == 0) {
                continue;
            }
            return -1;
        }

        // reading in
        if ((part == 3 || part == 4 || part == 5) && strcmp(value, "in") == 0) {
            if (foundIn == 0) {
                foundIn = 1;
                continue;
            }
            return -1;
        }

        // reading rackId
        if (foundIn == 1) {
            rackIdRead = value;
            rackIdRead[strlen(rackIdRead) - 1] = '\0';
            value = strtok(NULL, " ");
            if (value != NULL) {
                return -1;
            }
            continue;
        }

        if (switchCount == MAX_SWITCHES) {
            sprintf(response, "error rack limit exceeded");
            return 1;
        }
        switchesRead[switchCount] = value;
        switchCount += 1;
    }

    int switchesToBeAdded[3];
    for (int i = 0; i < switchCount; i++) {
        if (!isValidId(switchesRead[i])) {
            sprintf(response, "error switch type unknown");
            return 1;
        }
        switchesToBeAdded[i] = atoi(switchesRead[i]);
    }

    if (!isValidId(rackIdRead)) {
        sprintf(response, "error rack doesn't exist");
        return 1;
    }

    int rackId = atoi(rackIdRead);
    int rackIndex = findRack(rackId, racks);

    if (rackIndex == -1) {
        int freeRackIndex = findFreeRack(racks);
        if (freeRackIndex == -1) {
            sprintf(response, "error rack doesn't exist");
            return 1;
        }
        racks[freeRackIndex].id = rackId;
        return addSwitches(&racks[freeRackIndex], switchesToBeAdded, switchCount, response);
    }

    if (!hasAvailableSwitchSpace(&racks[rackIndex], switchCount)) {
        sprintf(response, "error rack limit exceeded");
        return 1;
    }

    if (hasAlreadyInstalledSwitches(&racks[rackIndex], switchesToBeAdded, switchCount)) {
        strcat(response, "error switch ");
        for (int i = 0; i < switchCount; i++) {
            char normalizedId[BUFSZ] = "";
            normalizeId(switchesToBeAdded[i], normalizedId);
            strcat(response, normalizedId);
            strcat(response, " ");
        }
        strcat(response, "already installed in ");
        char normalizedId[BUFSZ] = "";
        normalizeId(racks[rackIndex].id, normalizedId);
        strcat(response, normalizedId);
        return 1;
    }

    return addSwitches(&racks[rackIndex], switchesToBeAdded, switchCount, response);
}

int removeHandler(char *value, Rack racks[], char response[]) {
    int part = 0;
    int switchToBeRemoved = 0;
    char *rackIdRead = "0";
    while (value != NULL) {
        value = strtok(NULL, " ");
        part += 1;
        if (part == 1) {
            if (strcmp(value, "sw") == 0) {
                continue;
            }
            return -1;
        }
        if (part == 2) {
            switchToBeRemoved = atoi(value);
            if (switchToBeRemoved == 0) {
                return -1;
            }
            continue;
        }
        if (part == 3) {
            if (strcmp(value, "in") == 0) {
                continue;
            }
            return -1;
        }
        if (part == 4) {
            rackIdRead = value;
            rackIdRead[strlen(rackIdRead) - 1] = '\0';
            continue;
        }
        if (value == NULL) {
            continue;
        }
        return -1;
    }

    if (!isValidId(rackIdRead)) {
        sprintf(response, "error rack doesn't exist");
        return 1;
    }

    int rackId = atoi(rackIdRead);
    int rackIndex = findRack(rackId, racks);

    if (rackIndex == -1) {
        sprintf(response, "error rack doesn't exist");
        return 1;
    }

    for (int i = 0; i < MAX_SWITCHES; i++) {
        if (racks[rackIndex].switches[i] == switchToBeRemoved) {
            racks[rackIndex].switches[i] = 0;

            strcat(response, "switch ");
            char normalizedId[BUFSZ] = "";
            normalizeId(switchToBeRemoved, normalizedId);
            strcat(response, normalizedId);
            strcat(response, " removed from ");
            char normalizedRackId[BUFSZ] = "";
            normalizeId(racks[rackIndex].id, normalizedRackId);
            strcat(response, normalizedRackId);
            return 1;
        }
    }

    sprintf(response, "error switch doesn't exist");
    return 1;
}

int getHandler(char *value, Rack racks[], char response[]) {
    int part = 0;
    int switchesToBeReadFrom[2] = {0, 0};
    int switchCount = 0;
    char *rackIdRead = "0";
    int foundIn = 0;
    while (value != NULL) {
        value = strtok(NULL, " ");
        part += 1;

        // reading in
        if ((part == 2 || part == 3) && strcmp(value, "in") == 0) {
            if (foundIn == 0) {
                foundIn = 1;
                continue;
            }
            return -1;
        }

        // reading rackId
        if (foundIn == 1) {
            rackIdRead = value;
            rackIdRead[strlen(rackIdRead) - 1] = '\0';
            value = strtok(NULL, " ");
            if (value != NULL) {
                return -1;
            }
            continue;
        }

        if (switchCount == 2) {
            return -1;
        }
        switchesToBeReadFrom[switchCount] = atoi(value);
        switchCount += 1;
    }

    if (!isValidId(rackIdRead)) {
        sprintf(response, "error rack doesn't exist");
        return 1;
    }

    int rackId = atoi(rackIdRead);
    int rackIndex = findRack(rackId, racks);

    if (rackIndex == -1) {
        sprintf(response, "error rack doesn't exist");
        return 1;
    }

    int foundSwitches = 0;
    for (int i = 0; i < MAX_SWITCHES; i++) {
        for (int j = 0; j < switchCount; j++) {
            if (switchesToBeReadFrom[j] == 0) {
                sprintf(response, "error switch doesn't exist");
                return 1;
            }
            if (racks[rackIndex].switches[i] == switchesToBeReadFrom[j]) {
                foundSwitches++;
            }
        }
    }

    if (foundSwitches == switchCount) {
        for (int i = 0; i < switchCount; i++) {
            char randNumber[BUFSZ] = "";
            sprintf(randNumber, "%d", rand() % 10000);
            strcat(response, randNumber);
            strcat(response, " Kbs");
            if (i != (switchCount - 1)) {
                strcat(response, " ");
            }
        }
        return 1;
    }

    sprintf(response, "error switch doesn't exist");
    return 1;
}

int listHandler(char *value, Rack racks[], char response[]) {
    int part = 0;
    char *rackIdRead = "0";
    while (value != NULL) {
        value = strtok(NULL, " ");
        part += 1;
        if (part == 1) {
            rackIdRead = value;
            rackIdRead[strlen(rackIdRead) - 1] = '\0';
            continue;
        }
        if (value == NULL) {
            continue;
        }
        return -1;
    }

    if (!isValidId(rackIdRead)) {
        sprintf(response, "error rack doesn't exist");
        return 1;
    }

    int rackId = atoi(rackIdRead);
    int rackIndex = findRack(rackId, racks);

    if (rackIndex == -1) {
        sprintf(response, "empty rack");
        return 1;
    }

    int activeSwitchesCount = 0;
    for (int i = 0; i < MAX_SWITCHES; i++) {
        if (racks[rackIndex].switches[i] != 0) {
            activeSwitchesCount++;
        }
    }
    if (activeSwitchesCount == 0) {
        sprintf(response, "empty rack");
        return 1;
    }

    for (int i = 0; i < activeSwitchesCount; i++) {
        char normalizedId[BUFSZ] = "";
        normalizeId(racks[rackIndex].switches[i], normalizedId);
        strcat(response, normalizedId);
        if (i != (activeSwitchesCount - 1)) {
            strcat(response, " ");
        }
    }

    return 1;
}

int processMessage(char originalMessage[], Rack racks[], char response[]) {
    char message[BUFSZ];
    strcpy(message, originalMessage);
    char *value = strtok(message, " ");

    int command = getCommand(value);
    if (command == INVALID) {
        return -1;
    }
    if (command == ADD) {
        return addHandler(value, racks, response);
    }
    if (command == REMOVE) {
        return removeHandler(value, racks, response);
    }
    if (command == GET) {
        return getHandler(value, racks, response);
    }
    if (command == LIST) {
        return listHandler(value, racks, response);
    }
    return -1;
}

void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("exemple: %s v4 5151\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    srand(time(0));

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
    char message[BUFSZ];
    char response[BUFSZ];

    Rack racks[MAX_RACKS] = {
        [0] = {.id = 0, .switches = {0, 0, 0}},
        [1] = {.id = 0, .switches = {0, 0, 0}},
        [2] = {.id = 0, .switches = {0, 0, 0}},
        [3] = {.id = 0, .switches = {0, 0, 0}},
    };

    while (1) {
        int clientSocket = accept(serverSocket, caddr, &caddrlen);
        if (clientSocket == -1) {
            logexit("accept");
        }

        while (1) {
            memset(message, 0, BUFSZ);
            memset(response, 0, BUFSZ);

            int sentMessageLength = recv(clientSocket, message, BUFSZ - 1, 0);

            int result = processMessage(message, racks, response);
            if (result < 1) {
                close(clientSocket);
                break;
            }

            sentMessageLength = send(clientSocket, response, strlen(response) + 1, 0);
            if (sentMessageLength != strlen(response) + 1) {
                logexit("send");
            }
        }

        close(clientSocket);
    }

    exit(EXIT_SUCCESS);
}
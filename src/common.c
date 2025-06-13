#include "common.h"

void printExitCode(int exit_code){
    char* error_messages[] = {"SUCCESS",
                               "UNIDENTIFIED ERROR",
                               "ERROR IN SOCKET CREATION",
                               "ERROR IN ADDRESS PARSING",
                               "ERROR IN SOCKET BINDING",
                               "ERROR IN SOCKET LISTENING",
                               "ERROR IN RECEIVE",
                               "ERROR IN SEND",
                               "NO PEER ERROR",
                               "PEER LIMIT EXCEEDED",
                               "UNEXPECTED MESSAGE",
                               "ERROR IN SELECT",
                               "SENSOR LIMIT EXCEEDED",
                               "NO FREE SENSOR INDEX",
                               "ERROR IN CLIENT ACCEPT",
                               "ERROR IN PEER ACCEPT",
                               "PEER NOT FOUND IN DISCONNECT",
                               "SENSOR NOT FOUND IN DISCONNECT"};
    
    printf("Exiting: %s\n", error_messages[-exit_code]);
}

int stringToAddress(const char* address_string, const char* port_string, struct sockaddr_in* address){
    if (address_string == NULL || port_string == NULL)
        return -1;

    struct in_addr ipv4_address;
    uint16_t port = (uint16_t)atoi(port_string);
    if (port == 0) return -1;

    port = htons(port);
    if (inet_pton(AF_INET, address_string, &ipv4_address)){
        address->sin_family = AF_INET;
        address->sin_port = port;
        address->sin_addr = ipv4_address;
        return 0;
    }

    return -1;
}

int addressToString(const struct sockaddr_in* address, char* address_string, size_t string_size, uint16_t* port){
    if (address_string == NULL || address->sin_family != AF_INET) return -1;
    if (!inet_ntop(AF_INET, &(address->sin_addr), address_string, string_size)) return -1;

    *port = ntohs(address->sin_port);
    return 0;
}

void generateRandomID(uint8_t* id){
    for (int i = 0; i < ID_LENGTH; i++)
        id[i] = rand() % 10 + 48;
}

void printID(const uint8_t* id){
    for (int i = 0; i < ID_LENGTH; i++)
        printf("%c", (char)id[i]);
}

int compareIDs(const uint8_t* id1, const uint8_t* id2){
    for (int i = 0; i < ID_LENGTH; i++)
        if (id1[i] != id2[i]) return 0;
    
    return 1;
}

int createSocket(const char* address_string, const char* port_string, struct sockaddr_in* address){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1){
        fprintf(stderr, "Client socket initialization failed\n");
        return ERROR_SOCKET_CREATION;
    }

    if (stringToAddress(address_string, port_string, address)){
        fprintf(stderr, "Failed to interpret IPv4 address\n");
		return ERROR_ADDRESS_PARSING;
    }

    return s;
}

void closeSockets(int s1, int s2, int s3, int* socket_array){
    if (s1 != INACTIVE_SOCKET) close(s1);
    if (s2 != INACTIVE_SOCKET) close(s2);
    if (s3 != INACTIVE_SOCKET) close(s3);

    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
        if (socket_array == NULL) break;
        if (socket_array[i] != INACTIVE_SOCKET) close(socket_array[i]);
    }
}

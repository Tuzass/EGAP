#include "common.h"

int stringToAddress(const char* address_string, const char* port_string, struct sockaddr_in* address){
    if (address_string == NULL || port_string == NULL)
        return -1;

    struct in_addr ipv4_address;
    uint16_t port = (uint16_t)atoi(port_string);
    if (port == 0) return -1;

    // htons => host no network short
    // accounts for differences in endianness
    port = htons(port);

    // pton => presentation (string format) to network
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

    // ntop => network to presentation (string format)
    if (!inet_ntop(AF_INET, &(address->sin_addr), address_string, string_size)) return -1;
    *port = ntohs(address->sin_port);
    return 0;
}

void generateRandomID(uint8_t* ID){
    for (int i = 0; i < ID_LENGTH; i++)
        ID[i] = rand() % 10 + 48;
}

int compareIDs(const uint8_t* id1, const uint8_t* id2){
    for (int i = 0; i < ID_LENGTH; i++)
        if (id1[i] != id2[i]) return 0;
    
    return 1;
}

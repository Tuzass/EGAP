#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_MESSAGE_SIZE 500
#define MAX_BUFFER_SIZE MAX_MESSAGE_SIZE + 4
#define MAX_P2P_CONNECTIONS 1
#define MAX_CLIENT_SERVER_CONNECTIONS 15
#define MAX_IPv4_CHARACTER_LENGTH 15
#define INACTIVE_SOCKET -1

int stringToAddress(const char* address_string, const char* port_string, struct sockaddr_in* address_storage);
int addressToString(const struct sockaddr_in* address, char* address_string, size_t string_size, uint16_t* port);

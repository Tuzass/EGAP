#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_MESSAGE_SIZE 500
#define ID_LENGTH 10
#define MAX_BUFFER_SIZE MAX_MESSAGE_SIZE + 4
#define MAX_P2P_CONNECTIONS 1
#define MAX_CLIENT_SERVER_CONNECTIONS 15
#define INACTIVE_SOCKET -1

// message codes
#define MESSAGE_OK 0
#define REQ_CONNPEER 20
#define RES_CONNPEER 21
#define REQ_DISCPEER 22
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25
#define REQ_VALIDATEID 50
#define RES_VALIDATEID 51
#define MESSAGE_ERROR 255

// error message codes
#define PEER_LIMIT_EXCEEDED 1
#define PEER_NOT_FOUND 2
#define SENSOR_LIMIT_EXCEEDED 9
#define SENSOR_NOT_FOUND 10
#define LOCATION_NOT_FOUND 11

// successful message codes
#define SUCCESSFUL_DISCONNECT 1
#define SUCCESSFUL_CREATE 2
#define SENSOR_STATUS_OK 3
#define SUCCESSFUL_CONNECT 4
#define UNIQUE 60
#define NOT_UNIQUE 61

int stringToAddress(const char* address_string, const char* port_string, struct sockaddr_in* address_storage);
int addressToString(const struct sockaddr_in* address, char* address_string, size_t string_size, uint16_t* port);
void generateRandomID(uint8_t* id);
int compareIDs(const uint8_t* id1, const uint8_t* id2);

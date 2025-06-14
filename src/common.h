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

// constants
#define EXIT_LOGGING 0
#define MAX_MESSAGE_SIZE 500
#define ID_LENGTH 10
#define MAX_BUFFER_SIZE MAX_MESSAGE_SIZE + 1
#define MAX_P2P_CONNECTIONS 1
#define MAX_CLIENT_SERVER_CONNECTIONS 15
#define INACTIVE_SOCKET -1

// program error codes
#define ERROR_USAGE -1
#define ERROR_SOCKET_CREATION -2
#define ERROR_ADDRESS_PARSING -3
#define ERROR_SOCKET_BINDING -4
#define ERROR_SOCKET_LISTENING -5
#define ERROR_RECEIVE -6
#define ERROR_SEND -7
#define ERROR_NO_PEER -8
#define ERROR_PEER_LIMIT -9
#define ERROR_UNEXPECTED_MESSAGE -10
#define ERROR_SELECT -11
#define ERROR_SENSOR_LIMIT -12
#define ERROR_NO_FREE_INDEX -13
#define ERROR_CLIENT_ACCEPT -14
#define ERROR_PEER_ACCEPT -15
#define ERROR_PEER_NOT_FOUND -16
#define ERROR_SENSOR_NOT_FOUND -17
#define ERROR_LOCATION_NOT_FOUND -18
#define ERROR_AREA_NOT_FOUND -19

// protocol message codes
#define MESSAGE_OK 0
#define REQ_CONNPEER 20
#define RES_CONNPEER 21
#define REQ_DISCPEER 22
#define REQ_CONNSEN 23
#define RES_CONNSEN 24
#define REQ_DISCSEN 25
#define REQ_CHECKALERT 36
#define RES_CHECKALERT 37
#define REQ_SENSLOC 38
#define RES_SENSLOC 39
#define REQ_SENSSTATUS 40
#define RES_SENSSTATUS 41
#define REQ_LOCLIST 42
#define RES_LOCLIST 43
#define REQ_VALIDATEID 50
#define RES_VALIDATEID 51
#define REQ_SERVERIDENTITY 60
#define RES_SERVERIDENTITY 61
#define MESSAGE_ERROR 255

// protocol ERROR codes
#define PEER_LIMIT_EXCEEDED 1
#define PEER_NOT_FOUND 2
#define SENSOR_LIMIT_EXCEEDED 9
#define SENSOR_NOT_FOUND 10
#define LOCATION_NOT_FOUND 11

// protocol OK codes
#define SUCCESSFUL_DISCONNECT 1
#define SUCCESSFUL_CREATE 2
#define SENSOR_STATUS_OK 3
#define SUCCESSFUL_CONNECT 4
#define UNIQUE 60
#define NOT_UNIQUE 61
#define IDENTITY_STATUS 0
#define IDENTITY_LOCATION 1
#define IDENTITY_NONE 2

// location codes
#define NORTH 1
#define SOUTH 2
#define EAST 3
#define WEST 4

void printExitCode(int exit_code);
int stringToAddress(const char* address_string, const char* port_string, struct sockaddr_in* address_storage);
int addressToString(const struct sockaddr_in* address, char* address_string, size_t string_size, uint16_t* port);
void generateRandomID(uint8_t* id);
void printID(const uint8_t* id);
int compareIDs(const uint8_t* id1, const uint8_t* id2);
int createSocket(const char* address_string, const char* port_string, struct sockaddr_in* ipv4_address);
void closeSockets(int s1, int s2, int s3, int* socket_array);
int getAreaFromLocation(int location);
int getAreaName(int area, char* name);

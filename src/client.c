#include "common.h"

int main(int argc, char** argv){
    if (argc < 4) {
		fprintf(stderr,"Correct usage: %s <IP> <location_server_port> <status_server_port>\n", argv[0]);
		exit(0);
	}

    int s;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1){
        perror("Client socket initialization failed!\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in ipv4_address;
    if (stringToAddress(argv[1], argv[2], &ipv4_address)){
        fprintf(stderr,"Incorrect address format: expected an IPv4 address!\n");
		exit(0);
    }

    if (connect(s, (struct sockaddr*)(&ipv4_address), sizeof(struct sockaddr_in))){
        perror("Client connection failed!\n");
        exit(EXIT_FAILURE);
    }

    char address_string[MAX_IPv4_CHARACTER_LENGTH] = "";
    uint16_t port;
    if (addressToString(&ipv4_address, address_string, 16, &port)){
        perror("Address to string failed!\n");
        exit(EXIT_FAILURE);
    }

    close(s);
    printf("Correct execution, terminating...\n");
    return 0;
}

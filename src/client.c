#include "common.h"

// sets up a socket for an IP address and a port
// returns -1 if there was an error at any point
// returns the FD otherwise
int createSocket(const char* address_string, const char* port_string, struct sockaddr_in* ipv4_address){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1){
        perror("Client socket initialization failed!\n");
        return -1;
    }

    if (stringToAddress(address_string, port_string, ipv4_address)){
        fprintf(stderr,"Incorrect address format: expected an IPv4 address!\n");
		return -1;
    }

    return s;
}

// connects to the server specified in the socket and sends a greeting message
// if the server sends a response, it also prints it out
// returns -1 if the operation failed at any point
// returns 0 otherwise
int connectToServer(int socket, struct sockaddr_in* address){
    if (connect(socket, (struct sockaddr*)address, sizeof(struct sockaddr_in))){
        perror("Client connection failed!\n");
        return -1;
    }

    // char hello_message[MAX_BUFFER_SIZE];
    // create hello message...
    // send(socket, hello_message, strlen(hello_message), 0);

    // char hello_response[MAX_BUFFER_SIZE];
    // int response_length = recv(socket, hello_response, MAX_BUFFER_SIZE - 1, 0);

    return 0;
}

int main(int argc, char** argv){
    if (argc < 4){
		fprintf(stderr,"Correct usage: %s <IP> <location_server_port> <status_server_port>\n", argv[0]);
		exit(0);
	}

    struct sockaddr_in status_server_address;
    int status_server_socket = createSocket(argv[1], argv[2], &status_server_address);
    if (status_server_socket == -1){
        perror("Client socket initialization failed!\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in location_server_address;
    int location_server_socket = createSocket(argv[1], argv[3], &location_server_address);
    if (location_server_socket == -1){
        perror("Client socket initialization failed!\n");
        exit(EXIT_FAILURE);
    }

    int status_server_result = connectToServer(status_server_socket, &status_server_address);
    if (status_server_result == -1) printf("An error occured when connecting to server on port %s\n", argv[2]);

    int location_server_result = connectToServer(location_server_socket, &location_server_address);
    if (location_server_result == -1) printf("An error occured when connecting to server on port %s\n", argv[3]);

    fd_set readfds;
    char buffer[MAX_BUFFER_SIZE];
    int maxfd = (status_server_socket > location_server_socket ? status_server_socket : location_server_socket);

    while (1){
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin
        if (status_server_socket != INACTIVE_SOCKET) FD_SET(status_server_socket, &readfds);
        if (location_server_socket != INACTIVE_SOCKET) FD_SET(location_server_socket, &readfds);

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0){
            perror("Failed to wait on current connections!\n");
            break;
        }

        if (FD_ISSET(0, &readfds)){
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
            if (strncmp(buffer, "kill", 4) == 0) break;
            if (strncmp(buffer, "ss ", 3) == 0) send(status_server_socket, buffer + 3, strlen(buffer + 3), 0);
            else if (strncmp(buffer, "sl ", 3) == 0) send(location_server_socket, buffer + 3, strlen(buffer + 3), 0);
            else printf("Use 'ss <msg>' or 'sl <msg>' to send; use 'kill' to exit\n");
        }

        if (FD_ISSET(status_server_socket, &readfds)){
            int n = recv(status_server_socket, buffer, sizeof(buffer) - 1, 0);

            if (n <= 0){
                printf("Status server disconnected.\n");
                close(status_server_socket);
                status_server_socket = INACTIVE_SOCKET;
                continue;
            }

            buffer[n] = '\0';
            printf("[Status server] %s\n", buffer);
        }

        if (FD_ISSET(location_server_socket, &readfds)){
            int n = recv(location_server_socket, buffer, sizeof(buffer) - 1, 0);

            if (n <= 0){
                printf("Location server disconnected.\n");
                close(location_server_socket);
                location_server_socket = INACTIVE_SOCKET;
                continue;
            }

            buffer[n] = '\0';
            printf("[Location server] %s\n", buffer);
        }
    }

    printf("Closing client!\n");
    close(status_server_socket);
    close(location_server_socket);
    return 0;
}

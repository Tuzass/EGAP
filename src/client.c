#include "common.h"

uint8_t requestValidateID(int socket, uint8_t* validateid_request){
    uint8_t response[MAX_BUFFER_SIZE];

    if (send(socket, validateid_request, ID_LENGTH + 1, 0) == -1){
        fprintf(stderr, "Failed to send REQ_UNIQUEID\n");
        exit(EXIT_FAILURE);
    }

    int bytes_received = recv(socket, response, MAX_BUFFER_SIZE, 0);
    if (bytes_received <= 0){
        fprintf(stderr, "Failed to receive response or server disconnected\n");
        exit(EXIT_FAILURE);
    }

    if (response[0] != RES_VALIDATEID){
        fprintf(stderr, "Received an unexpected message\n");
        exit(EXIT_FAILURE);
    }

    return response[1];
}

void requestConnection(int socket, uint8_t* connection_request){
    uint8_t response[MAX_BUFFER_SIZE];

    if (send(socket, connection_request, ID_LENGTH + 1, 0) == -1){
        fprintf(stderr, "Failed to send REQ_UNIQUEID\n");
        exit(EXIT_FAILURE);
    }

    int bytes_received = recv(socket, response, MAX_BUFFER_SIZE, 0);
    if (bytes_received <= 0){
        fprintf(stderr, "Failed to receive response or server disconnected\n");
        exit(EXIT_FAILURE);
    }

    if (response[0] != RES_CONNSEN){
        fprintf(stderr, "Failed to connect to server\n");
        exit(EXIT_FAILURE);
    }
}

void sendValidID(int status_server_socket, int location_server_socket){
    uint8_t validateid_request[ID_LENGTH + 1];
    validateid_request[0] = REQ_VALIDATEID;
    
    while (1){
        generateRandomID(validateid_request + 1);

        uint8_t ss_response = requestValidateID(status_server_socket, validateid_request);
        if (ss_response == NOT_UNIQUE) continue;

        uint8_t ls_response = requestValidateID(location_server_socket, validateid_request);
        if (ls_response == NOT_UNIQUE) continue;

        break;
    }

    uint8_t connection_request[ID_LENGTH + 1];
    connection_request[0] = REQ_CONNSEN;
    for (int i = 0; i < ID_LENGTH; i++)
        connection_request[i + 1] = validateid_request[i + 1];
    
    requestConnection(status_server_socket, connection_request);
    requestConnection(location_server_socket, connection_request);
}

void connectToServer(int server_socket, struct sockaddr_in* server_address){
    if (connect(server_socket, (struct sockaddr*)server_address, sizeof(struct sockaddr_in))){
        fprintf(stderr, "Failed to connect to server\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    uint8_t response[MAX_BUFFER_SIZE];
    int received_bytes = recv(server_socket, response, MAX_BUFFER_SIZE, 0);
    if (received_bytes <= 0){
        fprintf(stderr, "Failed to receive response or server disconnected\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (response[0] == MESSAGE_ERROR && response[1] == SENSOR_LIMIT_EXCEEDED){
        fprintf(stderr, "Sensor limit exceeded\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv){
    if (argc < 4){
		fprintf(stderr,"Correct usage: %s <IP> <status_server_port> <location_server_port>\n", argv[0]);
		exit(0);
	}

    srand(time(NULL));

    struct sockaddr_in status_server_address;
    int status_server_socket = createSocket(argv[1], argv[2], &status_server_address);
    connectToServer(status_server_socket, &status_server_address);

    struct sockaddr_in location_server_address;
    int location_server_socket = createSocket(argv[1], argv[3], &location_server_address);
    connectToServer(location_server_socket, &location_server_address);

    sendValidID(status_server_socket, location_server_socket);

    fd_set readfds;
    char buffer[MAX_BUFFER_SIZE];
    int maxfd = (status_server_socket > location_server_socket ? status_server_socket : location_server_socket);

    while (1){
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin
        if (status_server_socket != INACTIVE_SOCKET) FD_SET(status_server_socket, &readfds);
        if (location_server_socket != INACTIVE_SOCKET) FD_SET(location_server_socket, &readfds);

        printf("Waiting on current connections\n");
        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0){
            fprintf(stderr, "Failed to wait on current connections\n");
            break;
        }
        printf("Something happened on ");

        if (FD_ISSET(0, &readfds)){
            printf("the terminal\n");
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;

            if (strncmp(buffer, "kill", 4) == 0){
                // sends a custom protocol message to warn server about disconnect
                break;
            }

            printf("Use 'kill' to exit\n");
        }

        if (FD_ISSET(status_server_socket, &readfds)){
            printf("the status server\n");
            int n = recv(status_server_socket, buffer, sizeof(buffer) - 1, 0);

            if (n <= 0){
                printf("Status server disconnected.\n");
                close(status_server_socket);
                status_server_socket = INACTIVE_SOCKET;
                continue;
            }
        }

        if (FD_ISSET(location_server_socket, &readfds)){
            printf("the location server\n");
            int n = recv(location_server_socket, buffer, sizeof(buffer) - 1, 0);

            if (n <= 0){
                printf("Location server disconnected.\n");
                close(location_server_socket);
                location_server_socket = INACTIVE_SOCKET;
                continue;
            }
        }
    }

    printf("Closing client\n");
    close(status_server_socket);
    close(location_server_socket);
    return 0;
}

#include "common.h"

int main(int argc, char** argv){
    if (argc < 4){
		fprintf(stderr,"Correct usage: %s <IP> <p2p_port> <client_port>\n", argv[0]);
		exit(0);
	}

    int client_listen_socket;
    int client_listen_opt = 1;
    client_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_listen_socket == -1){
        fprintf(stderr, "Failed to initialize the client listening socket\n");
        exit(EXIT_FAILURE);
    }
    setsockopt(client_listen_socket, SOL_SOCKET, SO_REUSEADDR, &client_listen_opt, sizeof(client_listen_opt));

    struct sockaddr_in client_address;
    if (stringToAddress(argv[1], argv[3], &client_address)){
        fprintf(stderr, "Failed to interpret IPv4 address");
		exit(0);
    }

    if (bind(client_listen_socket, (struct sockaddr*)(&client_address), sizeof(client_address))){
        fprintf(stderr, "Failed to bind client listening socket\n");
        exit(EXIT_FAILURE);
    }

    if (listen(client_listen_socket, MAX_CLIENT_SERVER_CONNECTIONS) == -1){
        fprintf(stderr, "Failed to listen on client port\n");
        close(client_listen_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in p2p_address;
    if (stringToAddress(argv[1], argv[2], &p2p_address)){
        fprintf(stderr, "Failed to interpret IPv4 address\n");
		exit(0);
    }

    int current_p2p_connections = 0;
    int current_client_connections = 0;
    socklen_t p2p_length = sizeof(p2p_address);
    socklen_t client_length = sizeof(client_address);

    int client_sockets[MAX_CLIENT_SERVER_CONNECTIONS]; // actual client connection sockets
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
        client_sockets[i] = INACTIVE_SOCKET;
    
    uint8_t client_ids[MAX_CLIENT_SERVER_CONNECTIONS * SENSOR_ID_LENGTH];
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS * SENSOR_ID_LENGTH; i++)
        client_ids[i] = 0;

    int p2p_listen_socket = INACTIVE_SOCKET; // socket where the server might listen on later
    int p2p_listen_opt = 1;
    int p2p_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (p2p_socket == -1) {
        fprintf(stderr, "Failed to create the P2P socket");
        exit(EXIT_FAILURE);
    }

    fd_set readfds;
    int maxfd;

    // main loop
    while (1){
        if (!current_p2p_connections && p2p_listen_socket == INACTIVE_SOCKET){
            if (!connect(p2p_socket, (struct sockaddr*)(&p2p_address), sizeof(struct sockaddr_in))){
                current_p2p_connections++;
                continue;
            }
            
            close(p2p_socket);
            p2p_socket = INACTIVE_SOCKET;

            p2p_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
            setsockopt(p2p_listen_socket, SOL_SOCKET, SO_REUSEADDR, &client_listen_opt, sizeof(client_listen_opt));
            if (p2p_listen_socket == -1){
                fprintf(stderr, "Failed to initialize P2P listening socket\n");
                exit(EXIT_FAILURE);
            }

            if (bind(p2p_listen_socket, (struct sockaddr*)(&p2p_address), sizeof(p2p_address))){
                fprintf(stderr, "Failed to bind P2P listening socket\n");
                exit(EXIT_FAILURE);
            }

            if (listen(p2p_listen_socket, MAX_P2P_CONNECTIONS) == -1){
                fprintf(stderr, "Failed to listen on the P2P port\n");
                close(p2p_listen_socket);
                exit(EXIT_FAILURE);
            }

            printf("No peer found, starting to listen...\n");
        }
        
        maxfd = 0;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin

        FD_SET(client_listen_socket, &readfds);
        if (client_listen_socket > maxfd) maxfd = client_listen_socket;

        if (p2p_socket != INACTIVE_SOCKET){
            FD_SET(p2p_socket, &readfds);
            if (p2p_socket > maxfd) maxfd = p2p_socket;
        }

        if (p2p_listen_socket != INACTIVE_SOCKET){
            FD_SET(p2p_listen_socket, &readfds);
            if (p2p_listen_socket > maxfd) maxfd = p2p_listen_socket;
        }
        
        for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
            if (client_sockets[i] != INACTIVE_SOCKET){
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > maxfd) maxfd = client_sockets[i];
            }
        }

        // printf("Waiting on current connections\n");
        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0){
            fprintf(stderr, "Failed to wait on current connections\n");
            exit(EXIT_FAILURE);
        }
        // printf("Something happened on ");

        // terminal input
        if (FD_ISSET(0, &readfds)){
            // printf("the terminal\n");
            char input_buffer[256];
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) break;

            if (strncmp(input_buffer, "kill", 4) == 0){
                // sends a custom protocol message to peer requesting to disconnect
                break;
            }

            // printf("Use 'kill' to exit\n");
        }

        // Client listening socket
        if (FD_ISSET(client_listen_socket, &readfds)){
            // printf("the client listening socket\n");
            struct sockaddr_in incoming_client_address;
            socklen_t incoming_client_length = sizeof(struct sockaddr_in);
            int new_client_socket = accept(client_listen_socket, (struct sockaddr*)&incoming_client_address, &incoming_client_length);

            if (new_client_socket == -1){
                fprintf(stderr, "Failed to accept an incoming client\n");
                exit(EXIT_FAILURE);
            }

            if (current_client_connections >= MAX_CLIENT_SERVER_CONNECTIONS){
                uint8_t sensor_limit_error_message[2] = {MESSAGE_ERROR, SENSOR_LIMIT_EXCEEDED};
                printf("Sensor limit exceeded\n");

                if (send(new_client_socket, sensor_limit_error_message, 2, 0) == -1){
                    fprintf(stderr, "Failed to warn sensor about sensor limit exceeded\n");
                    exit(EXIT_FAILURE);
                }

                close(new_client_socket);
                continue;
            }

            int free_index = -1;
            for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
                if (client_sockets[i] == INACTIVE_SOCKET){
                    free_index = i;
                    break;
                }
            }
            
            if (free_index == -1) {
                fprintf(stderr, "Failed to allocate slot for new client\n");
                close(new_client_socket);
                exit(EXIT_FAILURE);
            }

            uint8_t accepted_client_message[2] = {MESSAGE_OK, SUCCESSFUL_CONNECT};
            if (send(new_client_socket, accepted_client_message, 2, 0) == -1){
                fprintf(stderr, "Failed to warn sensor about successful connect\n");
                close(new_client_socket);
                exit(EXIT_FAILURE);
            }

            while (1){
                uint8_t validateid_request[SENSOR_ID_LENGTH + 1];
                int bytes_received = recv(new_client_socket, validateid_request, SENSOR_ID_LENGTH + 1, 0);

                if (bytes_received <= 0){
                    fprintf(stderr, "Failed to receive message from client or client disconnected\n");
                    close(new_client_socket);
                    exit(EXIT_FAILURE);
                }

                if (validateid_request[0] != REQ_VALIDATEID){
                    fprintf(stderr, "Received unexpected message\n");
                    close(new_client_socket);
                    exit(EXIT_FAILURE);
                }
                
                int unique_id = 1;

                for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
                    if (client_sockets[i] == INACTIVE_SOCKET) continue;

                    if (compareIDs(validateid_request + 1, &client_ids[i * SENSOR_ID_LENGTH])){
                        unique_id = 0;
                        break;
                    }
                }

                if (unique_id){
                    uint8_t validateid_response[2] = {RES_VALIDATEID, UNIQUE};

                    if (send(new_client_socket, validateid_response, 2, 0) == -1){
                        fprintf(stderr, "Failed to inform client about unique ID\n");
                        exit(EXIT_FAILURE);
                    }

                    break;
                }
            }

            uint8_t connection_request[SENSOR_ID_LENGTH + 1];
            int bytes_received = recv(new_client_socket, connection_request, SENSOR_ID_LENGTH, 0);
            if (connection_request[0] != REQ_CONNSEN){
                fprintf(stderr, "Received an unexpected message\n");
                exit(EXIT_FAILURE);
            }

            connection_request[0] = RES_CONNSEN;
            if (send(new_client_socket, connection_request, SENSOR_ID_LENGTH, 0) == -1){
                fprintf(stderr, "Failed to send RES_CONNSEN\n");
                exit(EXIT_FAILURE);
            }

            printf("Client ");
            for (int i = 0; i < SENSOR_ID_LENGTH; i++){
                client_ids[free_index * SENSOR_ID_LENGTH + i] = connection_request[i + 1];
                printf("%c", (char)connection_request[i + 1]);
            }
            printf(" added (Loc %d)\n", 0);

            client_sockets[free_index] = new_client_socket;
            current_client_connections++;
        }

        // P2P listening socket
        if (p2p_listen_socket != INACTIVE_SOCKET && FD_ISSET(p2p_listen_socket, &readfds)){
            // printf("the P2P listening socket\n");
            struct sockaddr_in incoming_p2p_address;
            socklen_t incoming_p2p_length = sizeof(incoming_p2p_address);
            int new_p2p_socket = accept(p2p_listen_socket, (struct sockaddr*)&incoming_p2p_address, &incoming_p2p_length);

            if (new_p2p_socket == -1){
                fprintf(stderr, "Failed to accept a new peer\n");
                exit(EXIT_FAILURE);
            }

            if (current_p2p_connections >= MAX_P2P_CONNECTIONS){
                uint8_t peer_limit_error_message[2] = {MESSAGE_ERROR, PEER_LIMIT_EXCEEDED};
                printf("Peer limit exceeded\n");

                if (send(new_p2p_socket, peer_limit_error_message, 2, 0) == -1){
                    fprintf(stderr, "Failed to warn peer about peer limit exceeded\n");
                    exit(EXIT_FAILURE);
                }

                close(new_p2p_socket);
                continue;
            }

            uint8_t accepted_peer_message[2] = {MESSAGE_OK, SUCCESSFUL_CONNECT};
            if (send(new_p2p_socket, accepted_peer_message, 2, 0) == -1){
                fprintf(stderr, "Failed to warn peer about successful connect\n");
                exit(EXIT_FAILURE);
            }
            
            p2p_socket = new_p2p_socket;
            current_p2p_connections++;
        }

        // P2P socket
        if (p2p_socket != INACTIVE_SOCKET && FD_ISSET(p2p_socket, &readfds)){
            // printf("the P2P socket\n");
            char incoming_message[MAX_BUFFER_SIZE];
            int bytes_received = recv(p2p_socket, incoming_message, sizeof(incoming_message) - 1, 0);

            if (bytes_received <= 0){
                // peer disconnected
                p2p_socket = INACTIVE_SOCKET;
                current_p2p_connections--;
                if (p2p_listen_socket != INACTIVE_SOCKET) printf("No peer found, starting to listen...\n");
                continue;
            }
        }

        // Client sockets
        for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
            if (client_sockets[i] != INACTIVE_SOCKET && FD_ISSET(client_sockets[i], &readfds)){
                // printf("client %d socket\n", i);
                char incoming_message[MAX_BUFFER_SIZE];
                int bytes_received = recv(client_sockets[i], incoming_message, sizeof(incoming_message) - 1, 0);
                
                if (bytes_received <= 0){
                    // client disconnected
                    printf("Client %d disconnected\n", i);
                    close(client_sockets[i]);
                    client_sockets[i] = INACTIVE_SOCKET;
                    current_client_connections--;
                    continue;
                }
            }
        }
    }

    close(client_listen_socket);
    if (p2p_listen_socket != INACTIVE_SOCKET) close(p2p_listen_socket);
    if (p2p_socket != INACTIVE_SOCKET) close(p2p_socket);

    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
        if (client_sockets[i] != INACTIVE_SOCKET){
            printf("Connection with client %d closed\n", i);
            close(client_sockets[i]);
            client_sockets[i] = INACTIVE_SOCKET;
        }
    }

    printf("Exiting...\n");
    return 0;
}

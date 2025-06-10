#include "common.h"

int main(int argc, char** argv){
    if (argc < 4) {
		fprintf(stderr,"Correct usage: %s <IP> <p2p_port> <client_port>\n", argv[0]);
		exit(0);
	}

    int client_listen_socket;
    int client_listen_opt = 1;
    client_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_listen_socket == -1){
        perror("Own socket initialization failed!\n");
        exit(EXIT_FAILURE);
    }
    setsockopt(client_listen_socket, SOL_SOCKET, SO_REUSEADDR, &client_listen_opt, sizeof(client_listen_opt));

    struct sockaddr_in client_address;
    if (stringToAddress(argv[1], argv[3], &client_address)){
        fprintf(stderr, "Incorrect address format. Expected an IPv4 address, received %s\n", argv[1]);
		exit(0);
    }

    if (bind(client_listen_socket, (struct sockaddr*)(&client_address), sizeof(client_address))){
        perror("Server connection failed!\n");
        exit(EXIT_FAILURE);
    }

    if (listen(client_listen_socket, MAX_CLIENT_SERVER_CONNECTIONS) == -1) {
        perror("Listen on client port failed!\n");
        close(client_listen_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in p2p_address;
    if (stringToAddress(argv[1], argv[2], &p2p_address)){
        fprintf(stderr, "Incorrect address format. Expected an IPv4 address, received %s\n", argv[1]);
		exit(0);
    }

    int current_p2p_connections = 0;
    int current_client_connections = 0;
    int started_p2p_listening = 0;
    socklen_t p2p_length = sizeof(p2p_address);
    socklen_t client_length = sizeof(client_address);

    int client_sockets[MAX_CLIENT_SERVER_CONNECTIONS]; // actual client connection sockets
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
        client_sockets[i] = 0;

    int p2p_listen_socket; // socket where the server might listen on later
    int p2p_listen_opt = 1;

    int p2p_socket = socket(AF_INET, SOCK_STREAM, 0); // actual P2P connection socket
    if (p2p_socket == -1){
        perror("Own socket initialization failed!\n");
        exit(EXIT_FAILURE);
    }

    // main loop
    while (1){
        // before listening on the P2P port, the server checks if some other server already is
        // if the connections succeeds, it won't listen in on the P2P port until the connection closes
        // if the connection fails, it is the only server for now, so it starts listening on the P2P port
        if (!current_p2p_connections && !started_p2p_listening){
            if (connect(p2p_socket, (struct sockaddr*)(&p2p_address), sizeof(struct sockaddr_in))){
                close(p2p_socket);
                p2p_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
                setsockopt(p2p_listen_socket, SOL_SOCKET, SO_REUSEADDR, &client_listen_opt, sizeof(client_listen_opt));
                if (p2p_listen_socket == -1){
                    perror("P2P socket initialization failed!\n");
                    exit(EXIT_FAILURE);
                }

                if (bind(p2p_listen_socket, (struct sockaddr*)(&p2p_address), sizeof(p2p_address))){
                    perror("Server connection failed!\n");
                    exit(EXIT_FAILURE);
                }

                if (listen(p2p_listen_socket, MAX_P2P_CONNECTIONS) == -1) {
                    perror("Listen on P2P port failed!\n");
                    close(p2p_listen_socket);
                    exit(EXIT_FAILURE);
                }

                started_p2p_listening = 1;
            }

            else{
                printf("Successfully connected to peer!\n");
                current_p2p_connections++;
            }
        }
        
        // the connection has failed, so it listens on the P2P port
        // accepts new connections through the IP address and ports informed
        // if a connection limit is already exceeded, sends an ERROR(01) message back before closing the socket
        if (started_p2p_listening && current_p2p_connections < MAX_P2P_CONNECTIONS){
            printf("Current P2P connections: %d\n", current_p2p_connections);
            printf("No peer found, starting to listen...\n");
            struct sockaddr_in incoming_p2p_address;
            socklen_t incoming_p2p_length = sizeof(incoming_p2p_address);
            p2p_socket = accept(p2p_listen_socket, (struct sockaddr*)&incoming_p2p_address, &incoming_p2p_length);
            current_p2p_connections++;
            printf("Successfully accepted connection from peer!\n");

            if (p2p_socket == -1){
                perror("Accept failed!\n");
                continue;
            }
        }

        // regardless of whether it listens on the P2P port or not, it will listen on its own client port
        if (current_client_connections < MAX_CLIENT_SERVER_CONNECTIONS) {
            printf("Waiting for a client to connect on port %s...\n", argv[3]);

            int free_index = -1;
            for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
                if (client_sockets[i] == 0) free_index = i;

            struct sockaddr_in incoming_client_address;
            int new_client_socket = accept(client_listen_socket, (struct sockaddr*)&incoming_client_address, &client_length);

            if (free_index == -1){
                printf("Client limit exceeded => connection refused!\n");
                send(client_sockets[free_index], "Client limit exceeded!", 22, 0);
                close(new_client_socket);
                continue;
            }
            
            else if (client_sockets[free_index] < 0) perror("Client accept failed!\n");

            else{
                client_sockets[free_index] = new_client_socket;
                printf("Client connected!\n");
                current_client_connections++;

                char buffer[1024] = {0};
                ssize_t bytes_received = recv(client_sockets[free_index], buffer, sizeof(buffer) - 1, 0);

                if (bytes_received > 0){
                    printf("Received from client: %s\n", buffer);
                    send(client_sockets[free_index], "Hello from server!", 18, 0);
                }

                if (strncmp(buffer, "close", 5) == 0){
                    close(client_sockets[free_index]);
                    client_sockets[free_index] = 0;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
        if (client_sockets[i] != 0){
            printf("Client connection closed.\n");
            close(client_sockets[i]);
            client_sockets[i] = 0;
        }
    }

    printf("Closing down!\n");
    close(p2p_socket);
    close(p2p_listen_socket);
    close(client_listen_socket);
    return 0;
}

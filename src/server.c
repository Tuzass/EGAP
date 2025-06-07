#include "common.h"

int main(int argc, char** argv){
    if (argc < 4) {
		fprintf(stderr,"Correct usage: %s <IP> <p2p_port> <client_port>\n", argv[0]);
		exit(0);
	}

    int client_socket;
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1){
        perror("Own socket initialization failed!\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_address;
    if (stringToAddress(argv[1], argv[3], &client_address)){
        fprintf(stderr, "Incorrect address format. Expected an IPv4 address, received %s\n", argv[1]);
		exit(0);
    }

    if (bind(client_socket, (struct sockaddr*)(&client_address), sizeof(client_address))){
        perror("Server connection failed!\n");
        exit(EXIT_FAILURE);
    }

    if (listen(client_socket, MAX_CLIENT_SERVER_CONNECTIONS) == -1) {
        perror("Listen on client port failed!\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    int p2p_socket;
    p2p_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (p2p_socket == -1){
        perror("P2P socket initialization failed!\n");
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

    // main loop
    while (1){
        if (!current_p2p_connections && !started_p2p_listening){
            printf("No P2P connections and not listening on port %s => attempting P2P connection\n", argv[2]);
            if (connect(p2p_socket, (struct sockaddr*)(&p2p_address), sizeof(struct sockaddr_in))){
                close(p2p_socket);
                int p2p_socket = socket(AF_INET, SOCK_STREAM, 0);
                if (p2p_socket == -1){
                    perror("P2P socket initialization failed!\n");
                    exit(EXIT_FAILURE);
                }

                if (bind(p2p_socket, (struct sockaddr*)(&p2p_address), sizeof(p2p_address))){
                    perror("Server connection failed!\n");
                    exit(EXIT_FAILURE);
                }

                if (listen(p2p_socket, MAX_P2P_CONNECTIONS) == -1) {
                    perror("Listen on P2P port failed!\n");
                    close(p2p_socket);
                    exit(EXIT_FAILURE);
                }

                started_p2p_listening = 1;
            }

            else{
                printf("Successfully connected to peer!\n");
                current_p2p_connections++;
            }
        }
        
        // accepts new connections through the IP address and ports informed
        // categorizes the connection (client or server) based on the first message received from it
        // if a connection limit is already exceeded, sends an ERROR(01) or ERROR(09) message back before closing the socket
        if (started_p2p_listening){
            printf("No peer found, starting to listen...\n");
            struct sockaddr_in incoming_p2p_address;
            socklen_t incoming_p2p_length = sizeof(incoming_p2p_address);
            int new_socket = accept(p2p_socket, (struct sockaddr*)&incoming_p2p_address, &incoming_p2p_length);
            printf("Successfully accepted connection from peer!\n");

            if (new_socket == -1){
                perror("Accept failed!\n");
                continue;
            }

            close(new_socket);
            printf("P2P connection closed!\n");
            current_p2p_connections--;
        }
    }

    printf("Correct execution, terminating...\n");
    return 0;
}

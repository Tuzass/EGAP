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

    if (listen(client_listen_socket, MAX_CLIENT_SERVER_CONNECTIONS) == -1){
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
    socklen_t p2p_length = sizeof(p2p_address);
    socklen_t client_length = sizeof(client_address);

    int client_sockets[MAX_CLIENT_SERVER_CONNECTIONS]; // actual client connection sockets
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
        client_sockets[i] = INACTIVE_SOCKET;

    int p2p_listen_socket = INACTIVE_SOCKET; // socket where the server might listen on later
    int p2p_listen_opt = 1;
    int p2p_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (p2p_socket == -1) {
        perror("P2P socket creation failed");
        exit(EXIT_FAILURE);
    }

    fd_set readfds;
    int maxfd;

    // main loop
    while (1){
        /* ===============================================================================================
            before listening on the P2P port, the server checks if some other server already is
            if the connections succeeds, it won't listen in on the P2P port until the connection closes
            if the connection fails, it is the only server for now, so it starts listening on the P2P port
        =============================================================================================== */

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
                perror("P2P socket initialization failed!\n");
                exit(EXIT_FAILURE);
            }

            if (bind(p2p_listen_socket, (struct sockaddr*)(&p2p_address), sizeof(p2p_address))){
                perror("Server connection failed!\n");
                exit(EXIT_FAILURE);
            }

            if (listen(p2p_listen_socket, MAX_P2P_CONNECTIONS) == -1){
                perror("Listen on P2P port failed!\n");
                close(p2p_listen_socket);
                exit(EXIT_FAILURE);
            }

            printf("No peer found, starting to listen...\n");
        }

        /* ===================================================================================
            sets up the socket to be monitored by select()
            these sockets include:
                 the terminal (stdin)
                 the client listening socket
                 the P2P listening socket, if this server is the one listening on the P2P port
                 the current client sockets
        =================================================================================== */
        
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

        /* =================================================================================================================
            the iteration stops here until something arrives in the sockets monitored by select() (or if there was an error)
            these sockets include:
                the terminal, which sends direct commands to the server
                the client listening socket, to accept and potentially refuse new client connections
                the P2P listening socket, if this server owns it, to accept and refuse other P2P connections
                the active P2P connection, if there is one
                the current clients connected to it
            
            if there was an error, this iteration is skipped and consecutive_select_fails is incremented
            if there are 3 consecutive select() fails, the program:
                sends an error message to every server and client connected to it
                closes the current active sockets
                shuts down
        ================================================================================================================= */

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0){
            perror("Select failed!\n");
            exit(EXIT_FAILURE);
        }

        /* =================================================================================================
            if select() returned, then something has arrived in one or more sockets
            from here on, the server must check which socket(s):
                if it was a terminal input (stdin), it processes the command and acts accordingly
                if it was a listening socket, it processes the incoming connection request and treats it
                if it was a client socket or the P2P, it processes the incoming message and acts accordingly
        ================================================================================================= */

        // terminal input
        if (FD_ISSET(0, &readfds)){
            char input_buffer[256];
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) break;

            if (strncmp(input_buffer, "kill", 4) == 0){
                // sends a custom protocol message to peer requesting to disconnect
                break;
            }

            else if (strncmp(input_buffer, "clients", 7) == 0){
                for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
                    if (client_sockets[i] != INACTIVE_SOCKET) send(client_sockets[i], input_buffer + 7, strlen(input_buffer + 7), 0);
            }

            else if (strncmp(input_buffer, "peer ", 5) == 0) send(p2p_socket, input_buffer + 5, strlen(input_buffer + 5), 0);
            else printf("Known commands: kill, peer\n");
        }

        // Client listening socket
        if (FD_ISSET(client_listen_socket, &readfds)){
            struct sockaddr_in incoming_client_address;
            socklen_t incoming_client_length = sizeof(struct sockaddr_in);
            int new_client_socket = accept(client_listen_socket, (struct sockaddr*)&incoming_client_address, &incoming_client_length);

            if (new_client_socket == -1){
                perror("Accept failed!\n");
                exit(EXIT_FAILURE);
            }

            if (current_client_connections >= MAX_CLIENT_SERVER_CONNECTIONS){
                // sends "Sensor limit exceeded" to the client and closes the socket
                printf("Sensor limit exceeded\n");
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
                fprintf(stderr, "No available slot for new client!\n");
                close(new_client_socket);
                continue;
            }
            
            client_sockets[free_index] = new_client_socket;
            current_client_connections++;
        }

        // P2P listening socket
        if (p2p_listen_socket != INACTIVE_SOCKET && FD_ISSET(p2p_listen_socket, &readfds)){
            struct sockaddr_in incoming_p2p_address;
            socklen_t incoming_p2p_length = sizeof(incoming_p2p_address);
            int new_p2p_socket = accept(p2p_listen_socket, (struct sockaddr*)&incoming_p2p_address, &incoming_p2p_length);

            if (new_p2p_socket == -1){
                perror("Accept failed!\n");
                exit(EXIT_FAILURE);
            }

            if (current_p2p_connections >= MAX_P2P_CONNECTIONS){
                // sends "Peer limit exceeded" to the server and closes the socket
                printf("Peer limit exceeded\n");
                close(new_p2p_socket);
                continue;
            }

            p2p_socket = new_p2p_socket;
            current_p2p_connections++;
        }

        // P2P socket
        if (p2p_socket != INACTIVE_SOCKET && FD_ISSET(p2p_socket, &readfds)){
            char incoming_message[MAX_BUFFER_SIZE];
            int bytes_received = recv(p2p_socket, incoming_message, sizeof(incoming_message) - 1, 0);

            if (bytes_received <= 0){
                // peer disconnected
                printf("Peer disconnected\n");
                p2p_socket = INACTIVE_SOCKET;
                current_p2p_connections--;
                printf("No peer found, starting to listen...\n");
                continue;
            }

            incoming_message[bytes_received] = '\0';
            printf("[Peer] %s\n", incoming_message);
        }

        // Client sockets
        for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
            if (client_sockets[i] != INACTIVE_SOCKET && FD_ISSET(client_sockets[i], &readfds)){
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
                
                incoming_message[bytes_received] = '\0';
                printf("[Client %d] %s\n", i, incoming_message);
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

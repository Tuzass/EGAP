#include "common.h"

int bindAndListen(int socket, struct sockaddr_in* address){
    if (bind(socket, (struct sockaddr*)address, sizeof(*address)) == -1) return ERROR_SOCKET_BINDING;
    if (listen(socket, MAX_CLIENT_SERVER_CONNECTIONS) == -1) return ERROR_SOCKET_LISTENING;
    return 0;
}

int createListenSocket(char* address_string, char* port_string, struct sockaddr_in* client_address){
    int socket = createSocket(address_string, port_string, client_address);
    if (socket < 0) return socket;

    int opt = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int rv = bindAndListen(socket, client_address);
    if (rv < 0) return rv;

    return socket;
}

int connectToPeer(int p2p_socket, struct sockaddr_in* p2p_address, int* has_generated_id, uint8_t* id, uint8_t* peer_id){
    if (connect(p2p_socket, (struct sockaddr*)p2p_address, sizeof(struct sockaddr_in)) == -1) return ERROR_NO_PEER;

    uint8_t response[MAX_BUFFER_SIZE];
    if (recv(p2p_socket, response, 2, 0) <= 0) return ERROR_RECEIVE;
    if (response[0] == MESSAGE_ERROR && response[1] == PEER_LIMIT_EXCEEDED) return ERROR_PEER_LIMIT;

    if (response[0] == MESSAGE_OK && response[1] == SUCCESSFUL_CONNECT){
        uint8_t peer_connection_request[ID_LENGTH];
        peer_connection_request[0] = REQ_VALIDATEID;

        while(1){
            if (*has_generated_id){
                for (int i = 0; i < ID_LENGTH; i++)
                    peer_connection_request[i + 1] = id[i];
            } else{
                generateRandomID(peer_connection_request + 1);
                *has_generated_id = 1;
            }
            
            if (send(p2p_socket, peer_connection_request, ID_LENGTH + 1, 0) == -1) return ERROR_SEND;
            if (recv(p2p_socket, response, ID_LENGTH + 1, 0) <= 0) ERROR_RECEIVE;

            if (response[0] == RES_VALIDATEID && response[1] == NOT_UNIQUE) continue;
            if (response[0] == RES_VALIDATEID && response[1] == UNIQUE) break;
        }

        printf("New Peer ID: ");
        for (int i = 0; i < ID_LENGTH; i++){
            id[i] = peer_connection_request[i + 1];
            printf("%c", (char)id[i]);
        }
        printf("\n");

        peer_connection_request[0] = REQ_CONNPEER;
        if (send(p2p_socket, peer_connection_request, ID_LENGTH + 1, 0) == -1) return ERROR_SEND;
        if (recv(p2p_socket, response, ID_LENGTH + 1, 0) <= 0) return ERROR_RECEIVE;

        if (response[0] == RES_CONNPEER){
            printf("Peer ");
            for (int i = 0; i < ID_LENGTH; i++){
                peer_id[i] = response[i + 1];
                printf("%c", (char)peer_id[i]);
            }
            printf(" connected\n");
        }

        return 0;
    }

    return ERROR_UNEXPECTED_MESSAGE;
}

void selectSetup(int* maxfd, fd_set* readfds, int client_listen_socket, int p2p_listen_socket, int p2p_socket, int* client_sockets){
    *maxfd = 0;
    FD_ZERO(readfds);
    FD_SET(0, readfds);

    FD_SET(client_listen_socket, readfds);
    if (client_listen_socket > *maxfd) *maxfd = client_listen_socket;

    if (p2p_socket != INACTIVE_SOCKET){
        FD_SET(p2p_socket, readfds);
        if (p2p_socket > *maxfd) *maxfd = p2p_socket;
    }

    if (p2p_listen_socket != INACTIVE_SOCKET){
        FD_SET(p2p_listen_socket, readfds);
        if (p2p_listen_socket > *maxfd) *maxfd = p2p_listen_socket;
    }
    
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
        if (client_sockets[i] != INACTIVE_SOCKET){
            FD_SET(client_sockets[i], readfds);
            if (client_sockets[i] > *maxfd) *maxfd = client_sockets[i];
        }
    }
}

int findFreeSensorIndex(int* client_sockets){
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
        if (client_sockets[i] == INACTIVE_SOCKET) return i;
    return ERROR_NO_FREE_INDEX;
}

int validateClientID(int new_client_socket, int* client_sockets, uint8_t* client_ids){
    while (1){
        uint8_t validateid_request[ID_LENGTH + 1];
        if (recv(new_client_socket, validateid_request, ID_LENGTH + 1, 0) <= 0){
            close(new_client_socket);
            return ERROR_RECEIVE;
        }

        if (validateid_request[0] != REQ_VALIDATEID){
            close(new_client_socket);
            return ERROR_UNEXPECTED_MESSAGE;
        }
        
        int unique_id = 1;

        for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
            if (client_sockets[i] == INACTIVE_SOCKET) continue;

            if (compareIDs(validateid_request + 1, &client_ids[i * ID_LENGTH])){
                unique_id = 0;
                break;
            }
        }

        if (unique_id){
            uint8_t validateid_response[2] = {RES_VALIDATEID, UNIQUE};

            if (send(new_client_socket, validateid_response, 2, 0) == -1){
                close(new_client_socket);
                return ERROR_SEND;
            }

            return 0;
        }
    }
}

int acceptClientConnection(int client_listen_socket, int current_client_connections, int* client_sockets, uint8_t* client_ids){
    struct sockaddr_in incoming_client_address;
    socklen_t incoming_client_length = sizeof(struct sockaddr_in);
    int new_client_socket = accept(client_listen_socket, (struct sockaddr*)&incoming_client_address, &incoming_client_length);
    if (new_client_socket == -1) return ERROR_CLIENT_ACCEPT;

    if (current_client_connections >= MAX_CLIENT_SERVER_CONNECTIONS){
        uint8_t sensor_limit_error_message[2] = {MESSAGE_ERROR, SENSOR_LIMIT_EXCEEDED};
        printf("Sensor limit exceeded\n");
        if (send(new_client_socket, sensor_limit_error_message, 2, 0) == -1) return ERROR_SEND;
        close(new_client_socket);
        return ERROR_SENSOR_LIMIT;
    }

    int free_index = findFreeSensorIndex(client_sockets);
    if (free_index == ERROR_NO_FREE_INDEX){
        close(new_client_socket);
        return ERROR_NO_FREE_INDEX;
    }

    uint8_t accepted_client_message[2] = {MESSAGE_OK, SUCCESSFUL_CONNECT};
    if (send(new_client_socket, accepted_client_message, 2, 0) == -1){
        close(new_client_socket);
        return ERROR_SEND;
    }

    int rv = validateClientID(new_client_socket, client_sockets, client_ids);
    if (rv){
        close(new_client_socket);
        return rv;
    }

    uint8_t req_connsen[ID_LENGTH + 1];
    if (recv(new_client_socket, req_connsen, ID_LENGTH + 1, 0) <= 0){
        close(new_client_socket);
        return ERROR_RECEIVE;
    }

    if (req_connsen[0] != REQ_CONNSEN){
        close(new_client_socket);
        return ERROR_UNEXPECTED_MESSAGE;
    }

    uint8_t res_connsen[ID_LENGTH + 1];
    res_connsen[0] = RES_CONNSEN;
    if (send(new_client_socket, res_connsen, ID_LENGTH + 1, 0) == -1){
        close(new_client_socket);
        return ERROR_SEND;
    }

    client_sockets[free_index] = new_client_socket;
    for (int i = 0; i < ID_LENGTH; i++)
        client_ids[free_index * ID_LENGTH + i] = req_connsen[i + 1];

    return free_index;
}

int validatePeerID(int new_p2p_socket, uint8_t* id){
    while(1){
        uint8_t validateid_request[ID_LENGTH + 1];
        if (recv(new_p2p_socket, validateid_request, ID_LENGTH + 1, 0) <= 0){
            close(new_p2p_socket);
            return ERROR_RECEIVE;
        }

        if (validateid_request[0] == REQ_VALIDATEID){
            if (compareIDs(id, validateid_request + 1)){
                uint8_t validateid_response[2] = {RES_VALIDATEID, NOT_UNIQUE};
                if (send(new_p2p_socket, validateid_response, 2, 0) == -1){
                    close(new_p2p_socket);
                    return ERROR_SEND;
                }

                continue;
            }

            uint8_t validateid_response[2] = {RES_VALIDATEID, UNIQUE};
            if (send(new_p2p_socket, validateid_response, 2, 0) == -1){
                close(new_p2p_socket);
                return ERROR_SEND;
            }

            return 0;
        }
    }
}

int acceptPeerConnection(int p2p_listen_socket, int current_p2p_connections, uint8_t* id, uint8_t* peer_id){
    struct sockaddr_in incoming_p2p_address;
    socklen_t incoming_p2p_length = sizeof(incoming_p2p_address);
    int new_p2p_socket = accept(p2p_listen_socket, (struct sockaddr*)&incoming_p2p_address, &incoming_p2p_length);

    if (new_p2p_socket == -1) return ERROR_PEER_ACCEPT;

    if (current_p2p_connections >= MAX_P2P_CONNECTIONS){
        uint8_t peer_limit_error_message[2] = {MESSAGE_ERROR, PEER_LIMIT_EXCEEDED};

        if (send(new_p2p_socket, peer_limit_error_message, 2, 0) == -1){
            close(new_p2p_socket);
            return ERROR_SEND;
        }

        close(new_p2p_socket);
        return ERROR_PEER_LIMIT;
    }

    uint8_t accepted_peer_message[2] = {MESSAGE_OK, SUCCESSFUL_CONNECT};
    if (send(new_p2p_socket, accepted_peer_message, 2, 0) == -1){
        close(new_p2p_socket);
        return ERROR_SEND;
    }

    int rv = validatePeerID(new_p2p_socket, id);

    uint8_t req_connpeer[ID_LENGTH + 1];
    if (recv(new_p2p_socket, req_connpeer, ID_LENGTH + 1, 0) <= 0){
        close(new_p2p_socket);
        return ERROR_RECEIVE;
    }

    if (req_connpeer[0] != REQ_CONNPEER){
        close(new_p2p_socket);
        return ERROR_UNEXPECTED_MESSAGE;
    }

    uint8_t res_connpeer[ID_LENGTH + 1];
    res_connpeer[0] = RES_CONNPEER;

    for (int i = 0; i < ID_LENGTH; i++){
        peer_id[i] = req_connpeer[i + 1];
        res_connpeer[i + 1] = id[i];
    }

    if (send(new_p2p_socket, res_connpeer, ID_LENGTH + 1, 0) == -1){
        close(new_p2p_socket);
        return ERROR_SEND;
    }

    return new_p2p_socket;
}

int acceptClientDisconnection(int client_socket, uint8_t* req_discsen, uint8_t* client_id){
    if (!compareIDs(client_id, req_discsen + 1)){
        uint8_t res_discsen[2] = {MESSAGE_ERROR, SENSOR_NOT_FOUND};
        if (send(client_socket, res_discsen, 2, 0) == -1) return ERROR_SEND;
        return SENSOR_NOT_FOUND;
    }

    uint8_t res_discsen[2] = {MESSAGE_OK, SUCCESSFUL_DISCONNECT};
    if (send(client_socket, res_discsen, 2, 0) == -1) return ERROR_SEND;
    return 0;
}

int requestPeerDisconnection(int p2p_socket, uint8_t* id){
    uint8_t req_discpeer[ID_LENGTH + 1];
    req_discpeer[0] = REQ_DISCPEER;
    for (int i = 0; i < ID_LENGTH; i++)
        req_discpeer[i + 1] = id[i];
    
    if (send(p2p_socket, req_discpeer, ID_LENGTH + 1, 0) == -1) return ERROR_SEND;

    uint8_t res_discpeer[2];
    if (recv(p2p_socket, res_discpeer, 2, 0) <= 0) return ERROR_RECEIVE;
    if (res_discpeer[0] == MESSAGE_ERROR && res_discpeer[1] == PEER_NOT_FOUND){
        printf("Peer not found\n");
        return ERROR_PEER_NOT_FOUND;
    }

    if (res_discpeer[0] == MESSAGE_OK && res_discpeer[1] == SUCCESSFUL_DISCONNECT){
        printf("Successful disconnect\n");
        return 0;
    }

    return ERROR_UNEXPECTED_MESSAGE;
}

int acceptPeerDisconnection(int p2p_socket, uint8_t* req_discpeer, uint8_t* peer_id){
    if (!compareIDs(peer_id, req_discpeer + 1)){
        uint8_t res_discpeer[2] = {MESSAGE_ERROR, PEER_NOT_FOUND};
        if (send(p2p_socket, res_discpeer, 2, 0) == -1) return ERROR_SEND;
        return PEER_NOT_FOUND;
    }

    uint8_t res_discpeer[2] = {MESSAGE_OK, SUCCESSFUL_DISCONNECT};
    if (send(p2p_socket, res_discpeer, 2, 0) == -1) return ERROR_SEND;
    return 0;
}

int main(int argc, char** argv){
    srand(time(NULL));

    if (argc < 4){
		fprintf(stderr, "Correct usage: %s <IP> <p2p_port> <client_port>\n", argv[0]);
        if (EXIT_LOGGING) printExitCode(ERROR_USAGE);
		exit(ERROR_USAGE);
	}
    
    struct sockaddr_in client_address;
    int client_listen_socket = createListenSocket(argv[1], argv[3], &client_address);
    if (client_listen_socket < 0){
        if (EXIT_LOGGING) printExitCode(client_listen_socket);
        exit(client_listen_socket);
    }

    struct sockaddr_in p2p_address;
    int p2p_socket = createSocket(argv[1], argv[2], &p2p_address);
    if (p2p_socket < 0){
        close(client_listen_socket);
        if (EXIT_LOGGING) printExitCode(p2p_socket);
        exit(p2p_socket);
    }

    int p2p_listen_socket = INACTIVE_SOCKET;
    int current_p2p_connections = 0;
    int current_client_connections = 0;

    int client_sockets[MAX_CLIENT_SERVER_CONNECTIONS];
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++)
        client_sockets[i] = INACTIVE_SOCKET;
    
    uint8_t client_ids[MAX_CLIENT_SERVER_CONNECTIONS * ID_LENGTH];
    for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS * ID_LENGTH; i++)
        client_ids[i] = 0;

    int has_generated_id = 0;
    uint8_t id[ID_LENGTH];
    uint8_t peer_id[ID_LENGTH];

    fd_set readfds;
    int maxfd;

    while (1){
        int should_attempt_peer_connection = !current_p2p_connections && p2p_listen_socket == INACTIVE_SOCKET;
        if (should_attempt_peer_connection){
            int rv = connectToPeer(p2p_socket, &p2p_address, &has_generated_id, id, peer_id);

            if (!rv) current_p2p_connections++;

            else if (rv == ERROR_NO_PEER){
                close(p2p_socket);
                p2p_socket = INACTIVE_SOCKET;

                p2p_listen_socket = createListenSocket(argv[1], argv[2], &p2p_address);

                if (!has_generated_id){
                    generateRandomID(id);
                    has_generated_id = 1;
                }

                printf("No peer found, starting to listen...\n");
            }

            else if (rv == ERROR_PEER_LIMIT){
                printf("Peer limit exceeded\n");
                if (EXIT_LOGGING) printExitCode(rv);
                exit(rv);
            }

            else{
                closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                if (EXIT_LOGGING) printExitCode(rv);
                exit(rv);
            }
        }
        
        selectSetup(&maxfd, &readfds, client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0){
            closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
            if (EXIT_LOGGING) printExitCode(ERROR_SELECT);
            exit(ERROR_SELECT);
        }

        if (FD_ISSET(0, &readfds)){
            char stdin_input[256];
            int kill = 0;
            int close_connection = 0;

            if (fgets(stdin_input, sizeof(stdin_input), stdin) == NULL) break;
            
            if (strncmp(stdin_input, "kill", 4) == 0) kill = 1;
            else if (strncmp(stdin_input, "close connection", 16) == 0) close_connection = 1;
            
            if (kill || close_connection){
                if (kill && current_p2p_connections == 0) break;
                if (close_connection && current_p2p_connections == 0){
                    printf("No peer connected to close connection\n");
                    continue;
                }

                int rv = requestPeerDisconnection(p2p_socket, id);
                if (rv){
                    closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                    if (EXIT_LOGGING) printExitCode(rv);
                    exit(rv);
                }

                printf("Peer ");
                printID(peer_id);
                printf(" disconnected\n");

                break;
            }

            printf("Use 'kill' to exit; If you don't want to exit in the case there is no peer, use 'close connection' instead \n");
        }

        if (FD_ISSET(client_listen_socket, &readfds)){
            uint8_t req_connsen[ID_LENGTH + 1];
            int new_client_index = acceptClientConnection(client_listen_socket, current_client_connections, client_sockets, client_ids);

            if (new_client_index >= 0){
                printf("Client ");
                printID(client_ids + new_client_index * ID_LENGTH);
                printf(" added (Loc %d)\n", 0);
                current_client_connections++;
            }
            
            else if (new_client_index == ERROR_SENSOR_LIMIT){
                printf("Sensor limit exceeded");
                continue;
            }

            else{
                closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                if (EXIT_LOGGING) printExitCode(new_client_index);
                exit(new_client_index);
            }
        }

        if (p2p_listen_socket != INACTIVE_SOCKET && FD_ISSET(p2p_listen_socket, &readfds)){
            p2p_socket = acceptPeerConnection(p2p_listen_socket, current_p2p_connections, id, peer_id);

            if (p2p_socket >= 0){
                printf("New Peer ID: ");
                printID(id);
                printf("\n");

                printf("Peer ");
                printID(peer_id);
                printf(" connected\n");
                
                current_p2p_connections++;
            }

            // the listening server apparently doesn't print "Peer limit exceeded"
            // but it's here in case it's ever needed

            /*
            else if (p2p_socket == ERROR_PEER_LIMIT) printf("Sensor limit exceeded");

            else{
                closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                if (EXIT_LOGGING) printExitCode(p2p_socket);
                exit(p2p_socket);
            }
            */

            else if (p2p_socket != ERROR_PEER_LIMIT){
                closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                if (EXIT_LOGGING) printExitCode(p2p_socket);
                exit(p2p_socket);
            }
        }

        if (p2p_socket != INACTIVE_SOCKET && FD_ISSET(p2p_socket, &readfds)){
            char incoming_message[MAX_BUFFER_SIZE];
            if (recv(p2p_socket, incoming_message, sizeof(incoming_message) - 1, 0) <= 0){
                closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                if (EXIT_LOGGING) printExitCode(ERROR_RECEIVE);
                exit(ERROR_RECEIVE);
            }

            if (incoming_message[0] == REQ_DISCPEER){
                int rv = acceptPeerDisconnection(p2p_socket, incoming_message, peer_id);

                if (!rv){
                    printf("Peer ");
                    printID(peer_id);
                    printf(" disconnected\n");

                    close(p2p_socket);
                    p2p_socket = INACTIVE_SOCKET;
                    current_p2p_connections--;
                    if (p2p_listen_socket != INACTIVE_SOCKET) printf("No peer found, starting to listen...\n");
                    continue;
                }

                closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                if (EXIT_LOGGING) printExitCode(rv);
                exit(rv);
            }
        }

        for (int i = 0; i < MAX_CLIENT_SERVER_CONNECTIONS; i++){
            if (client_sockets[i] != INACTIVE_SOCKET && FD_ISSET(client_sockets[i], &readfds)){
                char incoming_message[MAX_BUFFER_SIZE];
                if (recv(client_sockets[i], incoming_message, sizeof(incoming_message) - 1, 0) <= 0){
                    closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                    if (EXIT_LOGGING) printExitCode(ERROR_RECEIVE);
                    exit(ERROR_RECEIVE);
                }

                if (incoming_message[0] == REQ_DISCSEN){
                    int rv = acceptClientDisconnection(client_sockets[i], incoming_message, client_ids + i * ID_LENGTH);
                    if (!rv){
                        printf("Client ");
                        printID(client_ids + i * ID_LENGTH);
                        printf(" disconnected (Loc 0)\n");

                        close(client_sockets[i]);
                        client_sockets[i] = INACTIVE_SOCKET;
                        current_client_connections--;
                        continue;
                    }

                    closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
                    if (EXIT_LOGGING) printExitCode(rv);
                    exit(rv);
                }

                printf("Peer ");
                printID(peer_id);
                printf(" disconnected\n");
            }
        }
    }

    closeSockets(client_listen_socket, p2p_listen_socket, p2p_socket, client_sockets);
    if (EXIT_LOGGING) printExitCode(0);
    return 0;
}

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

void requestServerConnection(int socket, uint8_t* connection_request){
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

void sendValidID(int status_server_socket, int location_server_socket, uint8_t* id){
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
    for (int i = 0; i < ID_LENGTH; i++){
        id[i] = validateid_request[i + 1];
        connection_request[i + 1] = validateid_request[i + 1];
    }
    
    requestServerConnection(status_server_socket, connection_request);
    requestServerConnection(location_server_socket, connection_request);
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

int requestServerDisconnection(int socket, uint8_t* id){
    uint8_t req_discsen[ID_LENGTH + 1];
    req_discsen[0] = REQ_DISCSEN;
    for (int i = 0; i < ID_LENGTH; i++)
        req_discsen[i + 1] = id[i];

    if (send(socket, req_discsen, ID_LENGTH + 1, 0) == -1) return ERROR_SEND;

    uint8_t res_discsen[ID_LENGTH + 1];
    if (recv(socket, res_discsen, ID_LENGTH + 1, 0) <= 0) return ERROR_RECEIVE;

    if (res_discsen[0] == MESSAGE_OK && res_discsen[1] == SUCCESSFUL_DISCONNECT) return 0;
    if (res_discsen[0] == MESSAGE_ERROR && res_discsen[1] == SENSOR_NOT_FOUND) return ERROR_SENSOR_NOT_FOUND;
    return ERROR_UNEXPECTED_MESSAGE;
}

int requestSensorStatus(int status_server_socket){
    uint8_t req_sensstatus[1] = {REQ_SENSSTATUS};
    if (send(status_server_socket, req_sensstatus, 1, 0) == -1) return ERROR_SEND;

    int8_t res_sensstatus[2];
    if (recv(status_server_socket, res_sensstatus, 2, 0) <= 0) return ERROR_RECEIVE;

    if (res_sensstatus[0] == MESSAGE_OK && res_sensstatus[1] == SENSOR_STATUS_OK) return 0;
    if (res_sensstatus[0] == MESSAGE_ERROR && res_sensstatus[1] == SENSOR_NOT_FOUND) return ERROR_SENSOR_NOT_FOUND;
    if (res_sensstatus[0] == RES_SENSSTATUS) return res_sensstatus[1];
    return ERROR_LOCATION_NOT_FOUND;
}

int requestSensorLocation(int status_server_socket, uint8_t* req_sensloc){
    if (send(status_server_socket, req_sensloc, ID_LENGTH + 1, 0) == -1) return ERROR_SEND;

    int8_t res_sensloc[2];
    if (recv(status_server_socket, res_sensloc, 2, 0) <= 0) return ERROR_RECEIVE;

    if (res_sensloc[0] == MESSAGE_ERROR && res_sensloc[1] == SENSOR_NOT_FOUND) return ERROR_SENSOR_NOT_FOUND;
    if (res_sensloc[0] == RES_SENSLOC) return res_sensloc[1];
    return ERROR_LOCATION_NOT_FOUND;
}

int requestDiagnostic(int location_server_socket, uint8_t* req_loclist, uint8_t* sensor_id_list){
    if (send(location_server_socket, req_loclist, 3, 0) == -1) return ERROR_SEND;

    uint8_t res_loclist[MAX_CLIENT_SERVER_CONNECTIONS * ID_LENGTH + 1];
    int bytes_received = recv(location_server_socket, res_loclist, MAX_CLIENT_SERVER_CONNECTIONS * ID_LENGTH + 1, 0);

    if (bytes_received <= 0) return ERROR_RECEIVE;
    if (res_loclist[0] == MESSAGE_ERROR && res_loclist[1] == LOCATION_NOT_FOUND) return ERROR_LOCATION_NOT_FOUND;
    if (res_loclist[0] != RES_LOCLIST) return ERROR_UNEXPECTED_MESSAGE;
    if (res_loclist[0] == RES_LOCLIST && bytes_received % ID_LENGTH != 1) return ERROR_UNEXPECTED_MESSAGE;

    for (int i = 0; i < bytes_received; i++){
        sensor_id_list[i] = res_loclist[i + 1];
    }
    
    return bytes_received / ID_LENGTH;
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

    uint8_t id[ID_LENGTH];
    sendValidID(status_server_socket, location_server_socket, id);

    printf("SS New ID: ");
    printID(id);
    printf("\nSL New ID: ");
    printID(id);
    printf("\nSuccessful create\n");

    fd_set readfds;
    int maxfd = (status_server_socket > location_server_socket ? status_server_socket : location_server_socket);

    while (1){
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);
        if (status_server_socket != INACTIVE_SOCKET) FD_SET(status_server_socket, &readfds);
        if (location_server_socket != INACTIVE_SOCKET) FD_SET(location_server_socket, &readfds);

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0){
            fprintf(stderr, "Failed to wait on current connections\n");
            break;
        }

        if (FD_ISSET(0, &readfds)){
            char stdin_input[256];
            if (fgets(stdin_input, sizeof(stdin_input), stdin) == NULL) break;
            if (strncmp(stdin_input, "kill", 4) == 0){
                int ss_rv = requestServerDisconnection(status_server_socket, id);
                int ls_rv = requestServerDisconnection(location_server_socket, id);

                if (ss_rv == ERROR_SENSOR_NOT_FOUND && ls_rv == ERROR_SENSOR_NOT_FOUND){
                    printf("Sensor not found\n");
                    if (EXIT_LOGGING) printExitCode(ERROR_SENSOR_NOT_FOUND);
                    exit(ERROR_SENSOR_NOT_FOUND);
                }

                if (!ss_rv) printf("SS Successful disconnect\n");
                else if (ss_rv == ERROR_SENSOR_NOT_FOUND) printf("Sensor not found\n");
                else{
                    if (EXIT_LOGGING) printExitCode(ss_rv);
                    exit(ss_rv);
                }

                if (!ls_rv) printf("SL Successful disconnect\n");
                else if (ls_rv == ERROR_SENSOR_NOT_FOUND) printf("Sensor not found\n");
                else{
                    if (EXIT_LOGGING) printExitCode(ls_rv);
                    exit(ls_rv);
                }
                
                break;
            }

            if (strncmp(stdin_input, "check failure", 13) == 0){
                int status = requestSensorStatus(status_server_socket);

                if (status == ERROR_SENSOR_NOT_FOUND){
                    printf("Sensor not found\n");
                    if (EXIT_LOGGING) printExitCode(ERROR_SENSOR_NOT_FOUND);
                    break;
                }

                if (status == 0){
                    printf("Status do sensor 0\n");
                    continue;
                }

                if (status < 11){
                    char area_name[6];
                    int area = getAreaFromLocation(status);
                    getAreaName(area, area_name);

                    printf("Alert received from area: %d (%s)\n", area, area_name);
                    continue;
                }

                if (EXIT_LOGGING) printExitCode(status);
                break;
            }

            if (strncmp(stdin_input, "locate ", 7) == 0){
                uint8_t req_sensloc[ID_LENGTH + 1];
                req_sensloc[0] = REQ_SENSLOC;
                for (int i = 0; i < ID_LENGTH; i++)
                    req_sensloc[i + 1] = stdin_input[i + 7];

                int location = requestSensorLocation(location_server_socket, req_sensloc);

                if (location == ERROR_SENSOR_NOT_FOUND){
                    printf("Sensor not found\n");
                    if (EXIT_LOGGING) printExitCode(ERROR_SENSOR_NOT_FOUND);
                    break;
                }

                if (location > 0 && location < 11){
                    printf("Current sensor location: %d\n", location);
                    continue;
                }

                if (EXIT_LOGGING) printExitCode(location);
                break;
            }

            if (strncmp(stdin_input, "diagnose ", 9) == 0){
                uint8_t req_loclist[3];
                req_loclist[0] = REQ_LOCLIST;
                req_loclist[1] = stdin_input[9];
                req_loclist[2] = stdin_input[10];

                uint8_t sensor_id_list[MAX_CLIENT_SERVER_CONNECTIONS * ID_LENGTH];
                int num_sensors = requestDiagnostic(location_server_socket, req_loclist, sensor_id_list);
                if (num_sensors == ERROR_LOCATION_NOT_FOUND){
                    printf("Location not found\n");
                    if (EXIT_LOGGING) printExitCode(ERROR_LOCATION_NOT_FOUND);
                    break;
                }

                if (num_sensors == 0){
                    printf("No sensors at location %c%c\n", req_loclist[1], req_loclist[2]);
                    continue;
                }

                if (num_sensors >= 1 && num_sensors <= MAX_CLIENT_SERVER_CONNECTIONS){
                    printf("Sensors at location %c%c: ", req_loclist[1], req_loclist[2]);

                    for (int i = 0; i < num_sensors - 1; i++){
                        printID(sensor_id_list + i * ID_LENGTH);
                        printf(", ");
                    }

                    printID(sensor_id_list + (num_sensors - 1) * ID_LENGTH);
                    printf("\n");
                    continue;
                }

                if (EXIT_LOGGING) printExitCode(num_sensors);
                break;
            }

            printf("Use 'kill' to exit\n");
        }

        if (FD_ISSET(status_server_socket, &readfds)){
            uint8_t incoming_message[MAX_BUFFER_SIZE];
            if (recv(status_server_socket, incoming_message, sizeof(incoming_message) - 1, 0) <= 0){
                if (EXIT_LOGGING) printExitCode(ERROR_RECEIVE);
                break;
            }

            if (incoming_message[0] = REQ_SERVERIDENTITY){
                uint8_t res_serveridentity[2] = {RES_SERVERIDENTITY, IDENTITY_STATUS};
                if (send(status_server_socket, res_serveridentity, 2, 0) == -1){
                    if (EXIT_LOGGING) printExitCode(ERROR_SEND);
                    break;
                }
            }
        }

        if (FD_ISSET(location_server_socket, &readfds)){
            uint8_t incoming_message[MAX_BUFFER_SIZE];
            if (recv(location_server_socket, incoming_message, sizeof(incoming_message) - 1, 0) <= 0){
                close(location_server_socket);
                if (EXIT_LOGGING) printExitCode(ERROR_RECEIVE);
                exit(ERROR_RECEIVE);
            }

            if (incoming_message[0] = REQ_SERVERIDENTITY){
                uint8_t res_serveridentity[2] = {RES_SERVERIDENTITY, IDENTITY_LOCATION};
                if (send(location_server_socket, res_serveridentity, 2, 0) == -1){
                    if (EXIT_LOGGING) printExitCode(ERROR_SEND);
                    break;
                }
            }
        }
    }

    close(status_server_socket);
    close(location_server_socket);
    return 0;
}

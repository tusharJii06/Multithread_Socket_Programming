#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> 
#include <cstring>
using namespace std;

#define DEST_IP "127.0.0.1"      // IP of server where client establishes connection
#define DEST_PORT 8000           // PORT of the server(where the client eventually establishes connection)

void *listenAndPrintMessages(void *argv) {
    int *client_socket_fd = (int *)argv;
    char receiveBuffer[1024];
    while(true) {
        memset(receiveBuffer, 0, sizeof(receiveBuffer));
        ssize_t received_bytes = recv(*client_socket_fd, receiveBuffer, 1024, 0);
        if (received_bytes < 0)
        {
            printf("[ERROR]: Could not receive message\n");
            // break;
        }
        else if(received_bytes > 0)
        {
            printf("%s\n", receiveBuffer);
        } else {
            break;          // received_bytes = 0 means socket is closed
        }
    }
    close(*client_socket_fd);
    return (void *)client_socket_fd;
}

int main()
{
    // CREATE A CLIENT SOCKET
    int client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_fd < 0)
    {
        printf("[ERROR]: Unable to create socket\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[SUCCESS]: Socket created successfully\n");
    }

    // INITIALIZE sockaddr STRUCTURE(a container for all the info about destination socket(i.e server's socket not client's)) (https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html)
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);


    // CONNECT TO SERVER(connects the client's socket(using client_socket_fd to server's socket))
    // NOTE: IN ORDER TO CONNECT TO SERVER, WE DON'T NEED TO BIND CLIENT'S PORT TO ANY PORT, KERNEL WE ASSIGN A RANDOM PORT TO CLIENT SOCKET AND SEND THE INFO WITH THE CONNECTION REQUEST TO THE SERVER.
    // TODO: KEEP TRYING TO CONNECT THE CLIENT FOR 60 SECONDS.
    int connection_res = connect(client_socket_fd, (struct sockaddr*)&dest_addr, sizeof(sockaddr));
    if (connection_res < 0)
    {
        printf("[ERROR]: Unable to connect\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[SUCCESS]: Connection established\n");
    }

    // Get client(user) name
    char *name = NULL;
    size_t nameSize = 0;
    printf("What's your name?\n");
    ssize_t name_chars = getline(&name, &nameSize, stdin);       // get first and last name
    printf("Hello %s! You can start sending messages now\n", name);
    // capitalize the name
    name[name_chars - 1] = 0;           // to remove the '\n' from the end of the string(getline scans till it finds '\n')
    for(int i = 0; i < name_chars - 1; i++) {
        if(name[i] <= 'z' && name[i] >= 'a') {
            name[i] = 'A' + name[i] - 'a';
        }
    }

    // KEEP LISTENING TO INCOMING MESSAGES ON A SEPARATE THREAD(a thread is non-blocking code)
    pthread_t id;
    pthread_create(&id, NULL, listenAndPrintMessages, (void *)&client_socket_fd);       // see the parameter pthread_create takes. last param is the params required by the intermediate function in its void* argv

    // SEND AND RECEIVE MESSAGES FROM SERVER(no need to use separate thread, it can work on the main thread itself)
    // NOTE: send() system calls sends data to the FD mentioned in argument. Here, we are sending the data to client_socket_fd not server's fd because 1. client won't have server's fd and 2. the server has exact replica of client_socket_fd(as returned by accept sys call), and thus any message sent to client_socket_fd can be extracted from the same client_socket_fd present on the server side.
    const char *quit = "quit";
    char *message = NULL;
    size_t messageSize = 0;
    char buffer[1024];
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t total_chars = getline(&message, &messageSize, stdin);
        message[total_chars - 1] = 0;           // to remove the '\n' from the end of the string(getline scans till it finds '\n')
        sprintf(buffer, "[%s]: %s", name, message);     // sends formatted string in the buffer
        if(total_chars > 0) {
            if(strcmp(message, quit) == 0) {
                printf("[CLOSED]: Connection closed\n");
                break;
            }
            ssize_t bytes_sent = send(client_socket_fd, buffer, strlen(buffer), 0);   // returns total bytes sent successfully, may be less than strlen(message)
            if (bytes_sent < 0)
            {
                printf("[ERROR]: Could not send message, TYPE again\n");
                // break;
            }

        }
    }

    close(client_socket_fd);


    return 0;
}
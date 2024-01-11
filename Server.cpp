// https://www.gta.ufrj.br/ensino/eel878/sockets/index.html
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> 
#include <cstring>
#include <vector>
using namespace std;

#define PORT 8000
#define BACKLOG 10          // incoming connections are going to wait in this queue until you accept() them and this is the limit on how many can queue up
const char *quit = "quit";
vector<struct client_socket_info>acceptedClient;

struct client_socket_info {         // custom made structure to store all the information of a client socket on server side
    int client_socket_fd;           // fd of client socket on the server side
    int error;
    struct sockaddr_in client_socket_address;   // socket address of client socket on the server side
    bool acceptedSuccessfully;
};

struct client_socket_info *acceptNewConnection(int socket_fd) {     // accepts new connection on a new thread
    struct client_socket_info *new_client_socket = new struct client_socket_info;
    struct sockaddr_in newClientAddress;
    socklen_t clientAddressSize = sizeof(struct sockaddr);      // size of structure

    // returns a new FD corresponding to that particular client which it is connected to. It is differe FD from the original socket FD created at the start.
    // This is identical to the FD of client at the server side, and thus the server can communicate with this FD as if it is communicating with the client itself. 
    // This client_socket_fd is exactly the same FD as client_socket_fd in client.cpp and we send and receive messages from client through this FD only.
    int new_client_socket_fd = accept(socket_fd, (struct sockaddr*)&newClientAddress, &clientAddressSize);
    
    if (new_client_socket_fd < 0)
    {
        printf("[ERROR]: Could not accept connection from the client\n");
        new_client_socket->error = new_client_socket_fd;
        // exit(EXIT_FAILURE);
    }
    else
    {
        printf("[SUCCESS]: Connection established\n");
        new_client_socket->client_socket_fd = new_client_socket_fd;
        new_client_socket->client_socket_address = newClientAddress;
        new_client_socket->acceptedSuccessfully = true;
    }

    return new_client_socket;
}

int broadcast(int total_chars, char *writeBuffer, int exception_socket_fd = -1);

void *recvFromEachClientOnNewThread(void* argv) {
    struct client_socket_info *new_client_socket = (struct client_socket_info *)argv;
    char receiveBuffer[1024];
    while(true) {
        memset(receiveBuffer, 0, sizeof(receiveBuffer));
        ssize_t received_bytes = recv(new_client_socket->client_socket_fd, receiveBuffer, 1024, 0);
        if (received_bytes < 0)
        {
            printf("[ERROR]: Could not receive message\n");
        }
        else if(received_bytes > 0)
        {
            printf("%s\n", receiveBuffer);
            broadcast(received_bytes, receiveBuffer, new_client_socket->client_socket_fd);
        } else {
            break;          // received_bytes = 0 means socket is closed
        }   
    }
    close(new_client_socket->client_socket_fd);
    return (void *)new_client_socket;
}


void *newThreadForEachRequest(void *argv) {
    int *socket_fd = (int *)argv;
    while(1) {
        struct client_socket_info *new_client_socket = acceptNewConnection(*socket_fd);
        if(new_client_socket->acceptedSuccessfully) {
            acceptedClient.push_back(*new_client_socket);
            pthread_t id;
            pthread_create(&id, NULL, recvFromEachClientOnNewThread, (void *)new_client_socket);
        }
    }
}


int main()
{

    /* We do not need to initialize any environment for the socket as we are running in linux. In windows, we need environment initialization */

    // CREATE THE SOCKET
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // socket() is a system call that creates a socket/file and returns a file descriptor for the socket.
    // AF_INET(domain)->connection family. AF_INET is for communication between processes on different hosts connected by IPV4. AF_LOCAL for communication between processes on the same host.
    // SOCK_STREAM(type)->type of socket. SOCK_STREAM is for TCP(connection oriented), SOCK_DGRAM is for UDP(connectionless).
    // 0(protocol)-> just set protocol to "0" to have socket() choose the correct protocol based on the type

    if (socket_fd < 0)
    {
        printf("[ERROR]: Unable to create socket\n");
        exit(EXIT_FAILURE);     // #include <stdlib.h>
    }
    else
    {
        printf("[SUCCESS]: Socket created successfully\n");
    }

    // INITIALIZE sockaddr STRUCTURE(a container for all the info about our socket) (https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html)
    struct sockaddr_in serverAddress;         // instance of the sockaddr_in struct
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);     // htons() converts to Big-Endian notation. The port where server program will get bound to.
    serverAddress.sin_addr.s_addr = INADDR_ANY;       // INADDR_ANY will take system's ip address where the server file is running. Change this to the ip of the cloud server where this server file will be hosted in production(then our server file will be running on that cloud server).
    // serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");      // for a specific ip.


    // BIND THE SOCKET TO LOCAL PORT(if we don't bind, kernel will assign a random port number to the socket)
    // as there are many programs running on the same system with same ip(localhost)
    // hence our program needs to find a particular free port that it can run on(send and receive requests)
    int bind_res = bind(socket_fd, (struct sockaddr*)&serverAddress, sizeof(struct sockaddr));      // sockaddr_in and sockadrr are almost the same(same size and params), and hence the pointers can be typecasted to one another anytime. bind() accepts sockadrr pointer. It attaches(binds) the port to the socket.
    if (bind_res < 0)
    {
        printf("[ERROR]: Unable to bind socket\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[SUCCESS]: Socket bound successfully\n");
    }

    // LISTEN THE REQUEST FROM CLIENT(QUEUES THE REQUESTS)
    int listen_res = listen(socket_fd, BACKLOG);        // waits for incoming connections
    if (listen_res < 0)
    {
        printf("[ERROR]: Unable to listen\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[SUCCESS]: Listening for connections\n");
    }

    // NON BLOCKING CODE TO KEEP ACCEPTING REQUESTS(A NEW THREAD/PLATFORM TO ACCEPT REQUESTS AND EACH SEND\RECEIVE FROM A CLIENT WILL BE FURTHER ON A NEW THREAD).
    
    pthread_t id;
    pthread_create(&id, NULL, newThreadForEachRequest, (void *)&socket_fd);
    
    
    // INFINITE LOOP TO SEND MESSAGE TO ALL THE CLIENTS MULTIPLE TIMES

    char *writeBuffer = NULL;
    size_t writeBufferSize = 0;
    char buffer[1024];
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t total_chars = getline(&writeBuffer, &writeBufferSize, stdin);
        writeBuffer[total_chars - 1] = 0;           // to remove the '\n' from the end of the string(getline scans till it finds '\n')
        sprintf(buffer, "[SERVER]: %s", writeBuffer);
        int broadcast_res = broadcast(total_chars + 11, buffer);
        if(broadcast_res == -1) break;
    }

    close(socket_fd);

    return 0;
}

int broadcast(int total_chars, char *buffer, int exception_socket_fd) {

    // SEND TO ALL THE CLIENTS
    if(total_chars > 0) {
        if(strcmp(buffer, quit) == 0) {
            printf("[CLOSED]: Connection closed\n");
            return -1;
        }
        struct client_socket_info client;
        for(int i = 0; i < acceptedClient.size(); i++) {
            client = acceptedClient[i];
            if(!client.acceptedSuccessfully || client.client_socket_fd == exception_socket_fd) continue;
            ssize_t bytes_sent = send(client.client_socket_fd, buffer, strlen(buffer), 0);   // returns total bytes sent successfully, may be less than strlen(writeBuffer)
            if (bytes_sent < 0)
            {
                client.acceptedSuccessfully = false;
                printf("[ERROR]: Could not send message, to client %d\n", client.client_socket_fd);
                // break;
            }
        }
    }
    return 1;
}


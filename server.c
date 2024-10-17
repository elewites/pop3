/* server.c
 * Handles the creation of a server socket and data sending.
 * Author  : Jonatan Schroeder
 * Modified: Nov 6, 2021
 *
 * Modified by: Norm Hutchinson
 * Modified: Mar 5, 2022
 *
 * Notes: You will find useful examples in Beej's Guide to Network
 * Programming (http://beej.us/guide/bgnet/).
 */

#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>

int server_socket;

// Signal handler to gracefully close the server
void sigint_handler(int signum) {
    printf("Server shutting down\n");
    close(server_socket);
    exit(0);
}

/* TODO: Fill in the server code. You are required to listen on all interfaces for connections. For each connection,
 * invoke the handler on a new thread. */
void run_server(const char *port, void (*handler)(void *)) {
    struct sockaddr_in server_addr;
    
    // Set up signal handler to close the server gracefully
    // Possibly not necessary, but good practice
    signal(SIGINT, sigint_handler);

    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error opening socket");
        exit(1);
    }

    //configure the sockaddr_in struct
    server_addr.sin_family = AF_INET; // set to AF_INET to use IP
    server_addr.sin_port = htons(atoi(port)); // convert port to network byte order
    server_addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero)); // zero the rest of the struct

    // Bind the socket to the address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(1);
    }
    
    printf("Server bound to port %s\n", port);
    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("Error listening on socket");
        exit(1);
    }
    
    // Accept incoming connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }
        printf("Connection accepted from %s\n", inet_ntoa(client_addr.sin_addr));
        
        // Create a new thread to handle the connection
        pthread_t thread;
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = client_socket;
        if (pthread_create(&thread, NULL, (void *(*)(void *))handler, client_socket_ptr) != 0) {
            perror("Error creating thread");
            close(client_socket);
            free(client_socket_ptr);
        } else {
            pthread_detach(thread);
        }
    }
}



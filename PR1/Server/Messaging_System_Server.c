#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "buttons.h"

#define PORT 12345

int sock; // global for simplicity

// ----------------------------------
// Thread for button monitoring
// ----------------------------------
void *button_thread(void *arg) {
    int sock = *(int *)arg;
    Button_called(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    int socket_connection;
    struct sockaddr_in server;
    char buffer[256];

    // --- Create socket ---
    socket_connection = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_connection < 0) {
        perror("socket");
        return -1;
    }

    // --- Configure server address ---
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // --- Bind and listen ---
    if (bind(socket_connection, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind");
        close(socket_connection);
        return -1;
    }
    if (listen(socket_connection, 1) < 0) {
        perror("listen");
        close(socket_connection);
        return -1;
    }

    printf("Server ready on port %d. Waiting for client...\n", PORT);

    // --- Accept client ---
    sock = accept(socket_connection, NULL, NULL);
    if (sock < 0) {
        perror("accept");
        close(socket_connection);
        return -1;
    }

    printf("Client connected!\n");

    // --- Start button thread ---
    pthread_t button_tid;
    if (pthread_create(&button_tid, NULL, button_thread, &sock) != 0) {
        perror("pthread_create");
        close(sock);
        close(socket_connection);
        return -1;
    }

    // --- Main receive loop ---
    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int ret = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (ret <= 0) {
            if (ret == 0)
                printf("Client disconnected.\n");
            else
                perror("recv");
            break;
        }

        printf("[Client] %s", buffer);
    }

    close(sock);
    close(socket_connection);
    pthread_cancel(button_tid);
    pthread_join(button_tid, NULL);
    printf("Server closed.\n");
    return 0;
}


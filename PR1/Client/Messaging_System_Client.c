#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "buttons.h"

#define SERVER_IP "192.168.10.1"
#define PORT 12345

int sock; // global for simplicity

// ----------------------------------
// Thread function
// ----------------------------------
void *button_thread(void *arg) {
    int sock = *(int *)arg;
    Button_called(sock); 
    return NULL;
}

// ----------------------------------
// Wait until connection
// ----------------------------------
int wait_for_connection(void) {
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);

    printf("Attempting to connect to server %s:%d...\n", SERVER_IP, PORT);

    while (1) {
        int ret = connect(sock, (struct sockaddr *)&server, sizeof(server));
        if (ret == 0) {
            printf("Connected to server!\n");
            break;
        } else {
            printf("Connection failed: %s. Retrying in 1s...\n", strerror(errno));
            sleep(1);
        }
    }

    return sock;
}

// ----------------------------------
// MAIN
// ----------------------------------
int main(int argc, char *argv[]) {
    // 1. Wait until connected
    if (wait_for_connection() < 0) {
        fprintf(stderr, "Could not establish connection.\n");
        return -1;
    }

    // 2. Start button thread
    pthread_t button_tid;
    if (pthread_create(&button_tid, NULL, button_thread, &sock) != 0) {
        perror("pthread_create");
        close(sock);
        return -1;
    }

    // 3. Just listen for server replies (ACKs)
    char buffer[256];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int ret = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (ret <= 0) {
            printf("Server disconnected or error.\n");
            break;
        }
        printf("Received from server: %s", buffer);
    }

    close(sock);
    pthread_cancel(button_tid);
    pthread_join(button_tid, NULL);
    return 0;
}



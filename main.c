#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "publish.h"
#include "subscribe.h"

#define SERVER_IP "192.168.1.1"
#define PORT 1883

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    uint8_t packet[256];
    uint8_t buffer[256];

    // --- Create socket ---
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // --- Connect ---
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    printf("Connected to %s:%d\n", SERVER_IP, PORT);

    // --- Send CONNECT ---
    int len = build_connect(packet, "MQTT_Test");
    send(sock, packet, len, 0);
    printf("CONNECT sent (%d bytes)\n", len);

    // --- Wait for CONNACK ---
    int n = read(sock, buffer, sizeof(buffer));
    if (n >= 4 && buffer[0] == 0x20 && buffer[1] == 0x02 && buffer[3] == 0x00) {
        printf("CONNACK: Connection Accepted\n");
    } else {
        printf("CONNACK: Failed\n");
        close(sock);
        return -1;
    }

    // --- Publish message 1 ---
    len = build_publish(packet, "Rodrigo", "Testing topic: Rodrigo");
    send(sock, packet, len, 0);
    printf("PUBLISH sent (%d bytes)\n", len);
    
    // --- Publish message 2 ---
    len = build_publish(packet, "Daniel", "Testing topic: Daniel");
    send(sock, packet, len, 0);
    printf("PUBLISH sent (%d bytes)\n", len);
    
    // --- Publish message 3 ---
    len = build_publish(packet, "ITESO", "Testing topic: ITESO");
    send(sock, packet, len, 0);
    printf("PUBLISH sent (%d bytes)\n", len);

    // --- Subscribe to all topics ---
    subscribe_to_topic(sock, "Rodrigo");
    subscribe_to_topic(sock, "Daniel");
    subscribe_to_topic(sock, "ITESO");

    // --- Continuous read loop (keep alive) ---
    printf("Listening on topics: Rodrigo, Daniel, ITESO\n");

    while (1) {
        int n = read(sock, buffer, sizeof(buffer));
        if (n <= 0) {
            printf("Connection closed or lost.\n");
            break;
        }

        if ((buffer[0] & 0xF0) == 0x30) {  // PUBLISH frame
            int rem_len = buffer[1];
            int topic_len = (buffer[2] << 8) | buffer[3];

            char topic[64] = {0};
            memcpy(topic, &buffer[4], topic_len);

            int payload_start = 4 + topic_len;
            int payload_len = rem_len - (2 + topic_len);

            char payload[256] = {0};
            memcpy(payload, &buffer[payload_start], payload_len);

            printf("\n[MSG] Topic: %s\n", topic);
            printf("      Payload: %s\n", payload);
        }
    }
}


// MQTT_project.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <ctype.h>
#include <time.h>

#include "MQTT_project.h"

// ---------- MQTT Packet Builders ----------

int build_connect(uint8_t *packet, const char *clientID) {
    uint8_t *ptr = packet;
    int clientLen = strlen(clientID);

    *ptr++ = 0x10; // CONNECT
    int rem_len = 2 + 4 + 1 + 1 + 2 + 2 + clientLen;
    *ptr++ = rem_len;

    put_uint16(ptr, 4); ptr += 2;
    memcpy(ptr, "MQTT", 4); ptr += 4;
    *ptr++ = 0x04; // Protocol level 3.1.1
    *ptr++ = 0x02; // Clean session
    put_uint16(ptr, KEEP_ALIVE); ptr += 2;

    put_uint16(ptr, clientLen); ptr += 2;
    memcpy(ptr, clientID, clientLen); ptr += clientLen;

    return (ptr - packet);
}

int build_subscribe(uint8_t *packet, uint16_t pkt_id, const char *topic) {
    uint8_t *ptr = packet;
    int topicLen = strlen(topic);

    *ptr++ = 0x82; // SUBSCRIBE (QoS 1)
    int rem_len = 2 + 2 + topicLen + 1;
    *ptr++ = rem_len;

    put_uint16(ptr, pkt_id); ptr += 2;
    put_uint16(ptr, topicLen); ptr += 2;
    memcpy(ptr, topic, topicLen); ptr += topicLen;
    *ptr++ = QOS_LEVEL;

    return (ptr - packet);
}

int build_publish(uint8_t *packet, const char *topic, const char *msg) {
    uint8_t *ptr = packet;
    int topicLen = strlen(topic);
    int msgLen = strlen(msg);

    *ptr++ = 0x30; // PUBLISH, QoS 0
    int rem_len = 2 + topicLen + msgLen;
    *ptr++ = rem_len;

    put_uint16(ptr, topicLen); ptr += 2;
    memcpy(ptr, topic, topicLen); ptr += topicLen;
    memcpy(ptr, msg, msgLen); ptr += msgLen;

    return (ptr - packet);
}

int build_pingreq(uint8_t *packet) {
    packet[0] = 0xC0;
    packet[1] = 0x00;
    return 2;
}

// ---------- Message Parsing ----------

void handle_publish(uint8_t *buf, int len) {
    if (len < 4) return;
    int topicLen = (buf[2] << 8) | buf[3];
    char topic[64] = {0};
    memcpy(topic, &buf[4], topicLen);
    topic[topicLen] = '\0';

    int msgStart = 4 + topicLen;
    int msgLen = len - msgStart;
    char message[64] = {0};
    memcpy(message, &buf[msgStart], msgLen);
    message[msgLen] = '\0';

    printf("[MQTT] %s → %s\n", topic, message);
}

// ---------- Main ----------

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    uint8_t packet[256], buffer[256];
    fd_set readfds;
    struct timeval tv;
    time_t lastPing = time(NULL);

    // --- Socket setup ---
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(MQTT_SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(MQTT_SERVER_IP);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    printf("Connected to %s:%d\n", MQTT_SERVER_IP, MQTT_SERVER_PORT);

    // --- CONNECT ---
    int len = build_connect(packet, CLIENT_ID);
    send(sock, packet, len, 0);

    // Wait for CONNACK
    int n = read(sock, buffer, sizeof(buffer));
    if (n < 4 || buffer[0] != 0x20 || buffer[3] != 0x00) {
        printf("CONNACK failed\n");
        close(sock);
        return -1;
    }
    printf("MQTT connected.\n");

    // --- SUBSCRIBE to Door & Lights ---
    len = build_subscribe(packet, 1, TOPIC_DOOR);
    send(sock, packet, len, 0);
    read(sock, buffer, sizeof(buffer)); // SUBACK

    len = build_subscribe(packet, 2, TOPIC_LIGHTS);
    send(sock, packet, len, 0);
    read(sock, buffer, sizeof(buffer)); // SUBACK

    printf("Subscribed to topics: '%s', '%s'\n", TOPIC_DOOR, TOPIC_LIGHTS);
    printf("Type commands like: door open | light off\n");

    // --- Main loop ---
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(sock + 1, &readfds, NULL, NULL, &tv);
        (void)ret; // silence unused variable warning

        // Handle incoming MQTT packets
        if (FD_ISSET(sock, &readfds)) {
            n = read(sock, buffer, sizeof(buffer));
            if (n > 0 && (buffer[0] & 0xF0) == 0x30) { // PUBLISH
                handle_publish(buffer, n);
            }
        }

        // Handle user input
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[64];
            if (fgets(input, sizeof(input), stdin)) {
                // Parse input
                char cmd[16], val[16];
                if (sscanf(input, "%15s %15s", cmd, val) == 2) {
                    for (int i = 0; cmd[i]; ++i) cmd[i] = tolower(cmd[i]);
                    for (int i = 0; val[i]; ++i) val[i] = tolower(val[i]);

                    const char *topic = NULL;
                    if (strncmp(cmd, "door", 4) == 0)
                        topic = TOPIC_DOOR;
                    else if (strncmp(cmd, "light", 5) == 0)
                        topic = TOPIC_LIGHTS;

                    if (topic) {
                        len = build_publish(packet, topic, val);
                        send(sock, packet, len, 0);
                        printf("[LOCAL] Sent %s → %s\n", topic, val);
                    }
                }
            }
        }

        // --- Periodic PINGREQ every 30 s ---
        if (time(NULL) - lastPing > 30) {
            len = build_pingreq(packet);
            send(sock, packet, len, 0);
            lastPing = time(NULL);
        }
    }

    close(sock);
    return 0;
}


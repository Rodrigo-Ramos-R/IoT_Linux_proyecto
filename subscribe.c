// subscribe.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "subscribe.h"

int build_subscribe(uint8_t *packet, const char *topic) {
    uint8_t *ptr = packet;
    int topicLen = strlen(topic);

    *ptr++ = 0x82; // SUBSCRIBE (QoS 1)
    int rem_len = 2 + 2 + topicLen + 1;
    *ptr++ = rem_len;

    // Packet Identifier
    *ptr++ = 0x00;
    *ptr++ = 0x01;

    // Topic
    *ptr++ = (topicLen >> 8) & 0xFF;
    *ptr++ = topicLen & 0xFF;
    memcpy(ptr, topic, topicLen); ptr += topicLen;

    *ptr++ = 0x00; // QoS 0

    return (ptr - packet);
}

void subscribe_to_topic(int sock, const char *topic) {
    uint8_t packet[256];
    uint8_t buffer[256];

    int len = build_subscribe(packet, topic);
    send(sock, packet, len, 0);
    printf("SUBSCRIBE sent (%d bytes) to topic '%s'\n", len, topic);

    // Wait for SUBACK
    int n = read(sock, buffer, sizeof(buffer));
    if (n >= 5 && buffer[0] == 0x90) {
        printf("SUBACK received: Subscription to '%s' successful\n", topic);
    } else {
        printf("Failed to subscribe or bad response for '%s'\n", topic);
    }
}


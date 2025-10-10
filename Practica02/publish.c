#include <stdio.h>
#include <string.h>
#include "publish.h"

void put_uint16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

int build_connect(uint8_t *packet, const char *clientID) {
    uint8_t *ptr = packet;
    int clientLen = strlen(clientID);

    *ptr++ = 0x10; // CONNECT
    int rem_len = 2 + 4 + 1 + 1 + 2 + 2 + clientLen;
    *ptr++ = rem_len;

    // Variable header
    put_uint16(ptr, 4); ptr += 2;
    memcpy(ptr, "MQTT", 4); ptr += 4;
    *ptr++ = 0x04; // protocol level
    *ptr++ = 0x02; // clean session
    put_uint16(ptr, 60); ptr += 2;

    // Payload
    put_uint16(ptr, clientLen); ptr += 2;
    memcpy(ptr, clientID, clientLen); ptr += clientLen;

    return (ptr - packet);
}

int build_publish(uint8_t *packet, const char *topic, const char *msg) {
    uint8_t *ptr = packet;
    int topicLen = strlen(topic);
    int msgLen   = strlen(msg);

    *ptr++ = 0x30; // PUBLISH, QoS 0
    int rem_len = 2 + topicLen + msgLen;
    *ptr++ = rem_len;

    put_uint16(ptr, topicLen); ptr += 2;
    memcpy(ptr, topic, topicLen); ptr += topicLen;
    memcpy(ptr, msg, msgLen); ptr += msgLen;

    return (ptr - packet);
}


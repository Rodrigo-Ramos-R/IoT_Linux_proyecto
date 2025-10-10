#ifndef PUBLISH_H
#define PUBLISH_H

#include <stdint.h>

// Encode helpers
void put_uint16(uint8_t *buf, uint16_t val);

// MQTT packet builders
int build_connect(uint8_t *packet, const char *clientID);
int build_publish(uint8_t *packet, const char *topic, const char *msg);

#endif


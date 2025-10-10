#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H

#include <stdint.h>

int build_subscribe(uint8_t *packet, const char *topic);
void subscribe_to_topic(int sock, const char *topic);

#endif


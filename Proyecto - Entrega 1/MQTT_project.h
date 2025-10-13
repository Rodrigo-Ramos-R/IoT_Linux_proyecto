// MQTT_project.h
#ifndef MQTT_PROJECT_H
#define MQTT_PROJECT_H

#include <stdint.h>

#define MQTT_SERVER_IP   "192.168.0.246"
#define MQTT_SERVER_PORT 1883
#define CLIENT_ID        "LinuxMQTTClient"

#define TOPIC_DOOR       "Door"
#define TOPIC_LIGHTS     "Lights"

// QoS (0 = at most once, 1 = at least once)
#define QOS_LEVEL        0

// Keep alive (seconds)
#define KEEP_ALIVE       60

// Helper: encode 2-byte integer
static inline void put_uint16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

#endif


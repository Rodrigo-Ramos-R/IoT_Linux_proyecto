#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>

// =====================
// Compile-time defaults
// =====================
#ifndef MQTT_KEEP_ALIVE
#define MQTT_KEEP_ALIVE 60   // seconds
#endif

#ifndef MQTT_QOS_LEVEL
#define MQTT_QOS_LEVEL 0
#endif

// =====================
// Topics
// =====================
#define TOPIC_DOOR     "Door"
#define TOPIC_LIGHTS   "Light"

// =====================
// types
// =====================
typedef void (*MQTT_MessageCallback)(const char *topic,
                                     const char *payload,
                                     int payload_len);

// =====================
// API
// =====================

// Initialize internal state with broker IP/Port and an optional log prefix.
// Call once before Connect.
void MQTT_Init(const char *broker_ip, uint16_t port, const char *client_id);

// Provide a callback to be invoked on incoming PUBLISH messages.
void MQTT_SetMessageCallback(MQTT_MessageCallback cb);

// Connect to broker (blocking). Returns true on success.
bool MQTT_Connect(void);

// Subscribe to a topic (QoS 0 only). Returns true on success.
bool MQTT_Subscribe(const char *topic);

// Publish a message to topic (QoS 0). Returns true on success.
bool MQTT_Publish(const char *topic, const char *message);

// Start background loop thread that:
//  - receives MQTT packets (PUBLISH)
//  - sends PINGREQ periodically
// Returns true on success.
bool MQTT_LoopStart(void);

// Stop background loop thread and wait for it to exit.
void MQTT_LoopStop(void);

// Disconnect from broker and cleanup socket/thread.
void MQTT_Disconnect(void);

#endif // MQTT_H


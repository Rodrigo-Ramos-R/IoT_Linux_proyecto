#include "MQTT.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>

// =====================
// Internal state
// =====================
static int              s_sock = -1;
static struct sockaddr_in s_broker;
static char             s_client_id[64] = "pico-client";
static MQTT_MessageCallback s_msg_cb = NULL;

static volatile int     s_loop_running = 0;
static pthread_t        s_loop_thread;
static pthread_mutex_t  s_send_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint16_t         s_next_pkt_id = 1;

// =====================
// Helpers
// =====================
static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

// Minimal single-byte Remaining Length encoder/decoder.
// (valid for len < 127; extend if you need larger packets.)
static int put_rl_1b(uint8_t *p, int len) {
    p[0] = (uint8_t)len;
    return 1;
}

// =====================
// Packet builders
// =====================
static int build_connect(uint8_t *packet, const char *clientID) {
    uint8_t *ptr = packet;
    int clientLen = (int)strlen(clientID);

    *ptr++ = 0x10; // CONNECT (type=1, flags=0)
    int rem_len = 2 + 4 + 1 + 1 + 2 + 2 + clientLen; // var hdr + payload
    ptr += put_rl_1b(ptr, rem_len);

    // Protocol "MQTT"
    put_u16(ptr, 4);   ptr += 2;
    memcpy(ptr, "MQTT", 4); ptr += 4;

    // Protocol Level (4 = 3.1.1)
    *ptr++ = 0x04;

    // Connect Flags: Clean Session (bit1)
    *ptr++ = 0x02;

    // Keep Alive
    put_u16(ptr, MQTT_KEEP_ALIVE); ptr += 2;

    // Payload: Client ID
    put_u16(ptr, clientLen); ptr += 2;
    memcpy(ptr, clientID, clientLen); ptr += clientLen;

    return (int)(ptr - packet);
}

static int build_subscribe(uint8_t *packet, uint16_t pkt_id, const char *topic) {
    uint8_t *ptr = packet;
    int topicLen = (int)strlen(topic);

    *ptr++ = 0x82; // SUBSCRIBE, QoS 1 required by spec for SUBSCRIBE itself
    int rem_len = 2 + 2 + topicLen + 1; // pkt_id + topic + qos
    ptr += put_rl_1b(ptr, rem_len);

    put_u16(ptr, pkt_id); ptr += 2;
    put_u16(ptr, topicLen); ptr += 2;
    memcpy(ptr, topic, topicLen); ptr += topicLen;
    *ptr++ = MQTT_QOS_LEVEL; // requested QoS for topic

    return (int)(ptr - packet);
}

static int build_publish(uint8_t *packet, const char *topic, const char *msg) {
    uint8_t *ptr = packet;
    int topicLen = (int)strlen(topic);
    int msgLen   = (int)strlen(msg);

    *ptr++ = 0x30; // PUBLISH, QoS 0, no dup/retain
    int rem_len = 2 + topicLen + msgLen;
    ptr += put_rl_1b(ptr, rem_len);

    put_u16(ptr, topicLen); ptr += 2;
    memcpy(ptr, topic, topicLen); ptr += topicLen;
    memcpy(ptr, msg, msgLen);     ptr += msgLen;

    return (int)(ptr - packet);
}

static int build_pingreq(uint8_t *packet) {
    packet[0] = 0xC0;
    packet[1] = 0x00;
    return 2;
}

static int build_disconnect(uint8_t *packet) {
    packet[0] = 0xE0;
    packet[1] = 0x00;
    return 2;
}

// =====================
// Incoming handling
// =====================
static void handle_publish(uint8_t *buf, int len) {
    if (len < 4) return;

    // Fixed header:
    // byte0 = 0x30..0x3F (PUBLISH)
    // byte1 = Remaining Length (assumed 1B here)
    int rl = buf[1];
    if (rl + 2 > len) return; // malformed or truncated

    // Variable header: Topic
    int topicLen = get_u16(&buf[2]);
    if (topicLen + 2 > rl) return;

    // Topic string
    if (topicLen <= 0 || topicLen > 128) return;
    char topic[129] = {0};
    memcpy(topic, &buf[4], topicLen);
    topic[topicLen] = '\0';

    int payload_offset = 4 + topicLen;
    int payload_len = rl - (2 + topicLen); // RL - topicLenField(2) - topic
    if (payload_len < 0) payload_len = 0;

    char message[257] = {0};
    int copy_len = payload_len > 256 ? 256 : payload_len;
    memcpy(message, &buf[2 + 1 + payload_offset - 2], copy_len); // ensure from start of payload
    // payload starts at index: 2 (fixed hdr) + 2 (topicLen field) + topicLen
    int payload_abs = 2 + 2 + topicLen;
    memset(message, 0, sizeof(message));
    copy_len = (payload_abs + payload_len <= len) ? payload_len : (len - payload_abs);
    if (copy_len > 256) copy_len = 256;
    if (copy_len > 0) memcpy(message, &buf[payload_abs], copy_len);

    // Callback
    if (s_msg_cb) {
        s_msg_cb(topic, message, copy_len);
    } else {
        printf("[MQTT] %s → %.*s\n", topic, copy_len, message);
    }
}

// =====================
// Public API impl
// =====================
void MQTT_Init(const char *broker_ip, uint16_t port, const char *client_id) {
    memset(&s_broker, 0, sizeof(s_broker));
    s_broker.sin_family = AF_INET;
    s_broker.sin_port   = htons(port);
    inet_pton(AF_INET, broker_ip, &s_broker.sin_addr);

    if (client_id && *client_id) {
        strncpy(s_client_id, client_id, sizeof(s_client_id) - 1);
        s_client_id[sizeof(s_client_id) - 1] = '\0';
    }
}

void MQTT_SetMessageCallback(MQTT_MessageCallback cb) {
    s_msg_cb = cb;
}

bool MQTT_Connect(void) {
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }

    s_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (s_sock < 0) {
        perror("[MQTT] socket");
        return false;
    }

    if (connect(s_sock, (struct sockaddr *)&s_broker, sizeof(s_broker)) < 0) {
        perror("[MQTT] connect");
        close(s_sock);
        s_sock = -1;
        return false;
    }

    // Send CONNECT
    uint8_t pkt[256];
    int len = build_connect(pkt, s_client_id);
    if (write(s_sock, pkt, len) != len) {
        perror("[MQTT] send CONNECT");
        close(s_sock);
        s_sock = -1;
        return false;
    }

    // Wait for CONNACK
    uint8_t buf[256];
    int n = read(s_sock, buf, sizeof(buf));
    if (n < 4 || buf[0] != 0x20 || buf[3] != 0x00) {
        printf("[MQTT] CONNACK failed\n");
        close(s_sock);
        s_sock = -1;
        return false;
    }

    printf("[MQTT] Connected (client_id=%s)\n", s_client_id);
    return true;
}

bool MQTT_Subscribe(const char *topic) {
    if (s_sock < 0) return false;

    uint8_t pkt[256];
    uint16_t pkt_id = s_next_pkt_id++;
    int len = build_subscribe(pkt, pkt_id, topic);

    pthread_mutex_lock(&s_send_mutex);
    ssize_t w = write(s_sock, pkt, len);
    pthread_mutex_unlock(&s_send_mutex);
    if (w != len) {
        perror("[MQTT] send SUBSCRIBE");
        return false;
    }

    // Expect SUBACK (simple read; non-robust)
    uint8_t buf[256];
    int n = read(s_sock, buf, sizeof(buf));
    if (n < 5 || (buf[0] != 0x90)) {
        printf("[MQTT] SUBACK not received\n");
        return false;
    }
    printf("[MQTT] Subscribed to '%s'\n", topic);
    return true;
}

bool MQTT_Publish(const char *topic, const char *message) {
    if (s_sock < 0) return false;

    uint8_t pkt[256];
    int len = build_publish(pkt, topic, message);

    pthread_mutex_lock(&s_send_mutex);
    ssize_t w = write(s_sock, pkt, len);
    pthread_mutex_unlock(&s_send_mutex);

    if (w != len) {
        perror("[MQTT] send PUBLISH");
        return false;
    }
    return true;
}

static void *loop_thread_fn(void *arg) {
    (void)arg;
    time_t last_ping = time(NULL);

    while (s_loop_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s_sock, &rfds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int ret = select(s_sock + 1, &rfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(s_sock, &rfds)) {
            uint8_t buf[512];
            int n = read(s_sock, buf, sizeof(buf));
            if (n <= 0) {
                // Connection closed or error
                printf("[MQTT] socket closed\n");
                break;
            }

            // Handle simple packets
            uint8_t type = buf[0] & 0xF0;
            if (type == 0x30) {           // PUBLISH
                handle_publish(buf, n);
            } else if (type == 0xD0) {    // PINGRESP
                // ignore
            }
        }

        // Periodic PINGREQ
        if (time(NULL) - last_ping >= MQTT_KEEP_ALIVE / 2) {
            uint8_t ping[2];
            int len = build_pingreq(ping);
            pthread_mutex_lock(&s_send_mutex);
            (void)write(s_sock, ping, len);
            pthread_mutex_unlock(&s_send_mutex);
            last_ping = time(NULL);
        }
    }

    return NULL;
}

bool MQTT_LoopStart(void) {
    if (s_sock < 0) return false;
    if (s_loop_running) return true;

    s_loop_running = 1;
    if (pthread_create(&s_loop_thread, NULL, loop_thread_fn, NULL) != 0) {
        perror("[MQTT] pthread_create");
        s_loop_running = 0;
        return false;
    }
    return true;
}

void MQTT_LoopStop(void) {
    if (!s_loop_running) return;
    s_loop_running = 0;
    pthread_join(s_loop_thread, NULL);
}

void MQTT_Disconnect(void) {
    MQTT_LoopStop();

    if (s_sock >= 0) {
        uint8_t disc[2];
        int len = build_disconnect(disc);
        pthread_mutex_lock(&s_send_mutex);
        (void)write(s_sock, disc, len);
        pthread_mutex_unlock(&s_send_mutex);

        close(s_sock);
        s_sock = -1;
    }
    printf("[MQTT] Disconnected\n");
}


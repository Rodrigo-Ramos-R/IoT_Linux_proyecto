#include <stdio.h>
#include <unistd.h>
#include "MQTT.h"

// From Buttons.c
int Button_called(void);

// Callback for incoming MQTT messages (from phone → Pico)
static void on_mqtt_message(const char *topic, const char *payload, int len) {
    printf("[CB] Received message: Topic='%s' | Payload='%.*s'\n", topic, len, payload);
    // You can later link this to VoiceModel or GPIO actions
}

int main(void) {
    printf("====================================\n");
    printf("  IoT Voice Recognition Project\n");
    printf("====================================\n");

    // 1 MQTT initialization
    printf("[MAIN] Initializing MQTT client...\n");
    MQTT_Init("10.237.30.117", 1883, "pico-voice-ctrl");
    MQTT_SetMessageCallback(on_mqtt_message);

    // 2 Connect to MQTT broker
    printf("[MAIN] Attempting to connect to broker at 192.168.10.1:1883...\n");
    if (!MQTT_Connect()) {
        fprintf(stderr, "[ERROR] MQTT connection failed. Check Wi-Fi or broker.\n");
        return 1;
    }
    printf("[MAIN] Connected to MQTT broker.\n");

    // 3 Subscribe to control topics
    printf("[MAIN] Subscribing to topics...\n");
    if (MQTT_Subscribe(TOPIC_DOOR))
        printf("[MAIN] Subscribed to '%s'\n", TOPIC_DOOR);
    else
        printf("[MAIN] Failed to subscribe to '%s'\n", TOPIC_DOOR);

    if (MQTT_Subscribe(TOPIC_LIGHTS))
        printf("[MAIN] Subscribed to '%s'\n", TOPIC_LIGHTS);
    else
        printf("[MAIN] Failed to subscribe to '%s'\n", TOPIC_LIGHTS);

    // 4 Start MQTT background loop
    printf("[MAIN] Starting MQTT background loop...\n");
    if (MQTT_LoopStart())
        printf("[MAIN] MQTT loop started.\n");
    else
        printf("[MAIN] Failed to start MQTT loop.\n");

    // 5 Publish startup message
    printf("[MAIN] Publishing startup message...\n");
    if (MQTT_Publish(TOPIC_LIGHTS, "System_Started"))
        printf("[MAIN] Published startup message.\n");
    else
        printf("[MAIN] Failed to publish startup message.\n");

    // 6 Enter button handling loop
    printf("[MAIN] Entering button event loop. Press buttons on VoiceHat.\n");
    printf("[MAIN] (This loop runs indefinitely until program is stopped)\n");
    Button_called();

    // 7 Cleanup (unreachable unless Button_called exits)
    printf("[MAIN] Stopping MQTT loop and disconnecting...\n");
    MQTT_LoopStop();
    MQTT_Disconnect();

    printf("[MAIN] Program finished.\n");
    return 0;
}


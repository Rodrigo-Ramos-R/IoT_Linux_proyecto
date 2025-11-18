#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "MQTT.h"
#include "voice_recognition.h"

#define DEVICE "/dev/input/event0"

// Forward declaration
static void handle_voice_command(void);

int Button_called(void) {
    int fd;
    struct input_event ev;

    printf("[BUTTON] Listening to VoiceHat buttons...\n");

    fd = open(DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("[BUTTON] Cannot open input device");
        return 1;
    }

    while (1) {
        ssize_t n = read(fd, &ev, sizeof(struct input_event));
        if (n != sizeof(struct input_event))
            continue;

        if (ev.type == EV_KEY && ev.value == 1) {  // Key press
            switch (ev.code) {

                case 412:  // PREV → Voice Recognition
                    printf("[BUTTON] PREV pressed → Running Voice Recognition\n");

                    handle_voice_command();
                    usleep(200000);
                    break;

                case 207:  // PLAY
                    printf("PLAY pressed (start recording)\n");
                    system("arecord -D plughw:1,0 -r 48000 -f S16_LE -c 1 -d 3 Command.wav");
                    usleep(200000);
                    break;

                case 407:  // NEXT
                    printf("NEXT pressed (play recording)\n");
                    if (access("Command.wav", F_OK) == 0) {
                        system("aplay -D plughw:0,0 Command.wav");
                        usleep(200000);
                    } else {
                        printf("No recording found.\n");
                        usleep(200000);
                    }
                    break;


                case 113:  // MUTE
                    printf("[BUTTON] MUTE pressed (ignored)\n");
                    usleep(200000);
                    break;

                case 103:  // VOL+
                case 108:  // VOL-
                case 353:  // PAIR
                    printf("[BUTTON] Button code %d (no action)\n", ev.code);
                    usleep(200000);
                    break;

                default:
                    printf("[BUTTON] Unknown button code=%d\n", ev.code);
                    usleep(200000);
                    break;
            }
        }
    }

    close(fd);
    return 0;
}


// ===============================================
//  Voice Recognition Handler
// ===============================================
static void handle_voice_command(void) {

    /*
    if (access("Command.wav", F_OK) == 0) {
        printf("[VR] No audio file found. Press PLAY first.\n");
        return;
    } */

    printf("[VR] Running keyword recognizer...\n");

    printf("[VR-DEBUG] Before VR_Recognize_Command\n");
    int label = VR_Recognize_Command("Command.wav");
    printf("[VR-DEBUG] After VR_Recognize_Command, label=%d\n", label);

    if (label < 0) {
        printf("[VR] Recognition failed.\n");
        return;
    }

    switch (label) {
        case 0:
            MQTT_Publish("Light", "On");
            printf("[VR] MQTT → Light : On\n");
            break;
        case 1:
            MQTT_Publish("Light", "Off");
            printf("[VR] MQTT → Light : Off\n");
            break;
        case 2:
            MQTT_Publish("Door", "Open");
            printf("[VR] MQTT → Door : Open\n");
            break;
        case 3:
            MQTT_Publish("Door", "Close");
            printf("[VR] MQTT → Door : Close\n");
            break;
        default:
            printf("[VR] Unknown label %d\n", label);
            break;
    }
}

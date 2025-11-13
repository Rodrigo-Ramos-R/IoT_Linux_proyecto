#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#define DEVICE "/dev/input/event0"   // Adjust if needed (e.g., /dev/input/event1)

int Button_called(void) {
    int fd;
    struct input_event ev;

    printf("Listening to buttons: PREV, PLAY, NEXT, VOL+, VOL-, MUTE, PAIR...\n");

    fd = open(DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("Cannot open input device");
        return 1;
    }

    while (1) {
        ssize_t n = read(fd, &ev, sizeof(struct input_event));
        if (n != sizeof(struct input_event))
            continue;

        if (ev.type == EV_KEY && ev.value == 1) { // Key pressed
            switch (ev.code) {

                case 412:  // PREV
                    printf("PREV pressed - No command\n");
                    usleep(200000); // 200 ms
                    break;

                case 207:  // PLAY
                    printf("PLAY pressed (start recording)\n");
                    system("arecord -D plughw:1,0 -r 48000 -f S16_LE -c 1 -d 2 Command.wav &");
                    usleep(200000); // 200 ms
                    break;

                case 113:  // MUTE
                    printf("MUTE pressed - No command\n");
                    usleep(200000); // 200 ms
                    break;

                case 407:  // NEXT
                    printf("NEXT pressed (play recording)\n");
                    if (access("Command.wav", F_OK) == 0){
                        system("aplay -D plughw:0,0 Command.wav &");
                        usleep(200000); // 200 ms
                    }else{
                        printf("No recording found.\n");
                        usleep(200000); // 200 ms
                    }
                    break;

                case 103:  // VOL+
                    printf("VOL+ pressed - No command\n");
                    usleep(200000); // 200 ms
                    break;

                case 108:  // VOL-
                    printf("VOL- pressed - No command\n");
                    usleep(200000); // 200 ms
                    break;

                case 353:  // PAIR
                    printf("PAIR pressed - No command\n");
                    usleep(200000); // 200 ms
                    break;

                default:
                    printf("Unknown button (code=%d)\n", ev.code);
                    usleep(200000); // 200 ms
                    break;
            }
        }
    }

    close(fd);
    return 0;
}


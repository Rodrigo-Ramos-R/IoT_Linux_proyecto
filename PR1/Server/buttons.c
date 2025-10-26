#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string.h>
#include <sys/socket.h>   // for send()

#define DEVICE "/dev/input/event0"   // Change to /dev/input/event1 if needed

int Button_called(int sock) {
    int fd;
    struct input_event ev;

    printf("Escuchando botones: PREV, PLAY, NEXT, VOL+, VOL-, MUTE, PAIR...\n");

    fd = open(DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("No se pudo abrir el dispositivo de entrada");
        return 1;
    }

    while (1) {
        ssize_t n = read(fd, &ev, sizeof(struct input_event));
        if (n != sizeof(struct input_event))
            continue;

        if (ev.type == EV_KEY && ev.value == 1) { // PRESIONADO
            char msg[128] = {0};

            switch (ev.code) {

                // ==============================
                // PREV -> send the recorded file
                // ==============================
                case 412:
                    printf("PREV presionado\n");
                    system("scp ./mensaje.wav root@192.168.10.2:~");
                    snprintf(msg, sizeof(msg), "Voice note sent\n");
                    break;

                // ==============================
                // PLAY -> start recording
                // ==============================
                case 207:
                    printf("PLAY presionado (record start)\n");
                    system("./Grabar_mensaje.sh &");
                    snprintf(msg, sizeof(msg), "Recording voice note...\n");
                    break;

                // ==============================
                // MUTE -> stop recording
                // ==============================
                case 113:
                    printf("MUTE presionado (stop record)\n");
                    system("pkill Grabar_mensaje.");
                    system("pkill audio_capture");
                    snprintf(msg, sizeof(msg), "Recording stopped\n");
                    break;

                // ==============================
                // NEXT -> play received message
                // ==============================
                case 407:
                    printf("NEXT presionado (playback)\n");
                    system("./Reproducir_mensaje.sh");
                    snprintf(msg, sizeof(msg), "Playing received voice note\n");
                    break;

                // ==============================
                // Other buttons (info only)
                // ==============================
                case 103:
                    printf("VOL+ presionado\n");
                    snprintf(msg, sizeof(msg), "VOL+ presionado\n");
                    system("./vol_up.sh");
                    break;

                case 108:
                    printf("VOL- presionado\n");
                    snprintf(msg, sizeof(msg), "VOL- presionado\n");
                    system("./vol_down.sh");
                    break;

                case 353:
                    printf("PAIR presionado\n");
                    snprintf(msg, sizeof(msg), "PAIR presionado\n");
                    break;

                default:
                    printf("Botón desconocido (code=%d)\n", ev.code);
                    snprintf(msg, sizeof(msg), "Botón desconocido (%d)\n", ev.code);
                    break;
            }

            // Send the corresponding message through the socket
            if (send(sock, msg, strlen(msg), 0) < 0) {
                perror("send");
            }
        }
    }

    close(fd);
    return 0;
}

CC = gcc
CFLAGS = -Wall -Werror
OBJS = main.o publish.o subscribe.o

MQTT_test: $(OBJS)
	$(CC) $(CFLAGS) -o MQTT_test $(OBJS)

clean:
	rm -f $(OBJS) MQTT_test

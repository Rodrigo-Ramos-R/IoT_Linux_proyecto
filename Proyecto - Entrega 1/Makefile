# === Makefile for MQTT Project ===

# Use the ARM cross-compiler (adjust path as needed)
CC = aarch64-linux-gnu-gcc
# Or for 32-bit ARM: arm-linux-gnueabihf-gcc

CFLAGS = -Wall -Werror -static
OUTPUT = MQTT_project
SOURCES = MQTT_project.c
LIBS = 

# Default build rule
all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(OUTPUT) $(LIBS)

# Remove compiled binaries
clean:
	rm -f $(OUTPUT)

# Rebuild from scratch
fresh: clean all
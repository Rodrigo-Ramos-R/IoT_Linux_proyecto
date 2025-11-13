# ===========================================
#  IoT Voice Recognition MQTT Project
# ===========================================

# Toolchain (if cross-compiling, export CC before running make)
CC      ?= gcc
CFLAGS  ?= -Wall -O2
LIBS    = -lpthread

# --- Source files ---
MAIN_SRC = main.c MQTT.c buttons.c

# --- Output executable ---
MAIN_OUT = Voice_recognition_project

# --- Default build target ---
all: $(MAIN_OUT)

# --- Build rules ---
$(MAIN_OUT): $(MAIN_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# --- Clean and rebuild ---
clean:
	rm -f $(MAIN_OUT)

fresh: clean all

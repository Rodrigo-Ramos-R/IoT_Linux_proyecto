# ===========================================
#  IoT Messaging System Makefile
# ===========================================

CC      ?= $(CC)
CFLAGS  ?= -Wall -O2
LIBS    = -lpthread

# --- Source files ---
CLIENT_SRC = Messaging_System_Client.c buttons.c
SERVER_SRC = Messaging_System_Server.c buttons.c
BASE_SRC   = Base_application.c buttons.c

# --- Output executables ---
CLIENT_OUT = Messaging_System_Client
SERVER_OUT = Messaging_System_Server
BASE_OUT   = Base_application

# --- Default build target ---
all: $(CLIENT_OUT) $(SERVER_OUT) $(BASE_OUT)

# --- Build rules ---
$(CLIENT_OUT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(SERVER_OUT): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BASE_OUT): $(BASE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# --- Clean and rebuild ---
clean:
	rm -f $(CLIENT_OUT) $(SERVER_OUT) $(BASE_OUT)

fresh: clean all

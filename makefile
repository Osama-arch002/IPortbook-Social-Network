# ====================================================================
#  IPortbook Automation Assembly Layer
#  Targets: POSIX Threads & Networking Libraries on Linux (x86_64)
# ====================================================================

# Compiler configurations
CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

# Target binary executables
SERVER_EXE = server
CLIENT_EXE = client

# Build rules
all: $(SERVER_EXE) $(CLIENT_EXE)

$(SERVER_EXE): server.c protocool.h
	$(CC) $(CFLAGS) server.c -o $(SERVER_EXE)

$(CLIENT_EXE): client.c
	$(CC) $(CFLAGS) client.c -o $(CLIENT_EXE)

# Cleanup workspace binaries
clean:
	rm -f $(SERVER_EXE) $(CLIENT_EXE) *.o

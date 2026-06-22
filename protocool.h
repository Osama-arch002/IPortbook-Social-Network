#ifndef PROTOCOOL_H
#define PROTOCOOL_H


#include <stdint.h> // for uint16_t : unsigned int 16 bit


// 1. We define the types of messages a user can receive 
enum flussoType {
    FLUSSO_FRIEND_REQ = 0, // [0XX]
    FLUSSO_FRIEND_ACC = 1, // [1XX]
    FLUSSO_FRIEND_REJ = 2, // [2XX]
    FLUSSO_MESSAGE    = 3, // [3XX]
    FLUSSO_FLOOD      = 4  // [4XX]
};

// lista per I flussi, non array perche piu facile per la gestione, e non sappiamo quanti flussi possa ricevere quando e offline

struct flusso {

	enum flussoType type;
	char sender_id[9];
	char text[201];   //(Max 200 chars + \0) 
	
	struct flusso* next;       
};

// struct buffer session : Because of the TCP Stream Trap. Every active TCP socket needs its own temporary buffer. When read() fires, you add bytes to buffer[len].

struct session {
	char buffer[1024];
	int len;
};


struct user {
	
	char user_id[9];   // 8 characters + 1 for the null terminator'\0'
	int UDP_port;      // their UDP port for notifications
	uint16_t password;        // Password (0 to 65535)
	char ip[16]; 
	
	int tcp_socket;    // -1 if offline, or the FD if online!
	
	// The user's personal TCP stream buffer 
    struct session current_session;
	
	// The user's personal mailbox of flusso 
	struct flusso * flussoBox_head;
	struct flusso * flussoBox_tail;
	
	int unread_count;  // We need this for the UDP [YXX] notification!
	
	
};

#endif

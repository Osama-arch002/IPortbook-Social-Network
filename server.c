#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h> // Necessary for close()
#include <pthread.h>
#include <stdbool.h>
#include "protocool.h"

// my memory 
struct user users_array[100];


int friends[100][100]; // a 2d int array to know if 2 users are friends and if they have an unread message 0 = not friend , 1 = friends, 2 = they are friends and they have a message not read 


int total_users = 0;

pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;


// helper function check if id exist, return index if exist, -1 if no

int get_user_index(char *target_id) {
    for (int i = 0; i < total_users; i++) {
        if (strcmp(users_array[i].user_id, target_id) == 0) {
            return i;
        }
    }
    return -1; 
}




// helper function to add on linked lists of flussos 
void add_flusso_to_user(int target_idx, enum flussoType type, char *sender_id, char *text_msg) {
    
	// lock as soon we are adding 
	pthread_mutex_lock(&memory_lock);

	//Allocate space in the heap
    struct flusso *new_flusso = malloc(sizeof(struct flusso));
    
	if (new_flusso == NULL) {
        perror("Failed to allocate memory for flusso");
        return;
    }

    //fill the new box with data
    new_flusso->type = type;
    strcpy(new_flusso->sender_id, sender_id);
    
    memset(new_flusso->text, 0, sizeof(new_flusso->text));
    if (text_msg != NULL) {
        // Safe copy ensuring we don't overflow the 200 character limit
        strncpy(new_flusso->text, text_msg, 200);
    }
    
    new_flusso->next = NULL;

    // Attach
    if (users_array[target_idx].flussoBox_head == NULL) {
        // non c'e 
        users_array[target_idx].flussoBox_head = new_flusso;
        users_array[target_idx].flussoBox_tail = new_flusso;
    } else {
        // already c'e , attach to the tail
        users_array[target_idx].flussoBox_tail->next = new_flusso;
        users_array[target_idx].flussoBox_tail = new_flusso;
    }

    users_array[target_idx].unread_count++;

	// unlock 
	pthread_mutex_unlock(&memory_lock);
}


void send_udp(int target_idx, int flusso_type) {
    // creare un socket temporanea per udp (notifiche)
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    //  gestire l'address prendendo la porta dal array dei clienti
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(users_array[target_idx].UDP_port);
    target_addr.sin_addr.s_addr = inet_addr(users_array[target_idx].ip);
    
    // 3. 3-byte message (decimal ex '3' and Hex "01")   ,since it is hexadecimal the biggest number of flussi I can have is 255 as it's only 2 bytes,after that it becomes 3 bytes 
    char alert_msg[4];
    sprintf(alert_msg, "%d%02X", flusso_type, users_array[target_idx].unread_count);
    
    // 4. Throw it at the client!
    sendto(udp_sock, alert_msg, 3, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
    
    // 5. Destroy the socket
    close(udp_sock);
}


// flood functions 

void flood_recursive(int original_sender, int current_idx, char *mess, bool *visited) {
    for(int i = 0; i < total_users; ++i) {
        if(friends[current_idx][i] == 1 && visited[i] == 0) {
            visited[i] = true; 
            add_flusso_to_user(i, FLUSSO_FLOOD, users_array[original_sender].user_id, mess);

			// notifica udp 
			send_udp(i, FLUSSO_FLOOD);

			// chiamata ricorsiva
            flood_recursive(original_sender, i, mess, visited);
        }
    }
}

void flood_msg(int sender_idx, char *mess) {
    bool visited[100] = {false}; // set them all to false 
    visited[sender_idx] = true; 
    flood_recursive(sender_idx, sender_idx, mess, visited);
}



//---------------------------------------------------------------------------
// main() only handles the very first handshake (accept). The moment the connection is established, main() hands the socket key over to handle_client and walks away

struct client_data {
    int fd;
    char ip_str[16];
};


// thread function 

void *handle_client(void *envelope) {
    
    
    struct client_data *data= (struct client_data *)envelope;
    int conversation_fd = data->fd;
    char client_ip_string[16];
    strcpy(client_ip_string, data->ip_str);
    
	free(data); // Clean up memory
    
    // a buffer to hold the incoming message
    char buff[512];
    		
    int logged_in_idx = -1; 
    
    char pending_friend_id[9] = ""; // To remember who just asked to be friends!
    
    printf("New thread started for socket %d!\n", conversation_fd);

    
    while(1) {
    	
    	// clear the buffer 
    	memset(buff, 0, sizeof(buff));
    	
    	// read returns the byte that insert in the buffer
        int bytes_read = read(conversation_fd, buff, sizeof(buff)-1);
        // -1 to always leave room for the null terminator!
        
        if (bytes_read <= 0) break; // Client disconnected
        

		// check if terminator exists 
		char *terminator = NULL;
        for (int i = 0; i <= bytes_read - 3; i++) {
            if (buff[i] == '+' && buff[i+1] == '+' && buff[i+2] == '+') {
                terminator = &buff[i];
                break;
            }
        }
            
        if (terminator == NULL) {
            printf("Rejected: Message did not end with +++\n");
            write(conversation_fd, "GOBYE+++\n", 9);
            break;
        }
            	
        int total_message_length = terminator - buff;  // ??
    			
    	// REGIS--------------------------
    	if (strncmp (buff,"REGIS",5) == 0){
    				
			if (logged_in_idx != -1) {
            	printf("Client is already logged in. Terminating connection.\n");
                write(conversation_fd, "GOBYE+++\n", 9);
                break; 
        	}

    		char temp_id [9];  //8 bytes + 1 for \0
    		char temp_port[5]; // 4 bytes + \0
    		int stop = 0;      // To find where the text stops
    				
    		//  Extract the text parts (ID and Port)
    				
    		// %8s : 8 chars , %4s : 4 chars (port)
    		// %n will tell us exactly where the port
    		// finishes in the buffer
    		int items_found = sscanf (buff, "REGIS %8s %4s ",temp_id,temp_port);
			
			// Fast-forward past the space!
    		stop = 6 + strlen(temp_id) + 1 + strlen(temp_port) + 1;

    		// If it didn't find BOTH the ID and the Port, reject them!
    		// make sure the password exists :  checking if the bytes between port and +++ is 2 bytes
    		if (items_found != 2 || (total_message_length - stop != 2)) {
				write(conversation_fd, "GOBYE+++\n", 9);
				break;
    		}
    				
			// Extract the binary password using memcpy				
    		uint16_t temp_mdp = 0;
    		
    		// technically we copy 2 bytes after stop to our temp_mdp
    		memcpy(&temp_mdp, &buff[stop], 2);
			stop += 2;

			if (strncmp(&buff[stop], "+++", 3) != 0) {
    			printf("Rejected: Missing +++ after password\n");
    			write(conversation_fd, "GOBYE+++\n", 9);
   				break;
			}
    				
    		int real_port = atoi (temp_port);

			if (real_port < 1024 || real_port > 9999) {
                printf("Registration rejected: Port %d is out of range.\n", real_port);
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
            }
    		
			// lock 
			pthread_mutex_lock(&memory_lock);

    		// CHECK IF ID ALREADY EXIST 
    				
    		if (get_user_index(temp_id) != -1) {
    			printf("Registration rejected: ID '%s'  already exists.\n", temp_id);
    			write(conversation_fd, "GOBYE+++\n", 9);
				break;
			}
    				
    		strcpy(users_array[total_users].user_id,temp_id);
    		users_array[total_users].UDP_port = real_port;
			users_array[total_users].password = temp_mdp;

			strcpy(users_array[total_users].ip, client_ip_string);
    				
			logged_in_idx = total_users;

    		total_users ++;
    		
			// unlock
			pthread_mutex_unlock(&memory_lock);

    		write(conversation_fd, "WELCO+++\n", 9);
    				
    		// test
    		printf("Registered: %s | Port: %d | Password Code: %u\n", temp_id, real_port, temp_mdp);
    				
    	}else if (strncmp (buff,"CONNE",5) == 0){
    		// CONNECT ( log in)-----------
    				
			if (logged_in_idx != -1) {
                printf("Rejected: Client is already logged in. Terminating connection.\n");
                write(conversation_fd, "GOBYE+++\n", 9);
                break; 
            }
    		char temp_id [9];
    		
    				
    		int items_found = sscanf(buff, "CONNE %8s ", temp_id);
    		
			int stop = 6 + strlen(temp_id) + 1; // "CONNE " (6 bytes) + ID length + space (1 byte)

    		if (items_found != 1  || (total_message_length - stop != 2)) {
        		write(conversation_fd, "GOBYE+++\n", 9);
        		break;
    		}

    		uint16_t temp_mdp = 0;

    		memcpy(&temp_mdp, &buff[stop], 2);
			stop += 2;

			if (strncmp(&buff[stop], "+++", 3) != 0) {
    			write(conversation_fd, "GOBYE+++\n", 9);
    			break;
			}
    				
    		// check if they already exist
    				
    		int connect_succeed = 0;
			int user_index = get_user_index(temp_id); // Find exactly where they are!

			// If they exist & their password match
			if (user_index != -1 && users_array[user_index].password == temp_mdp) {
					
				connect_succeed = 1;
			}
    				
    		// loop finish either found or no 
    				
    		if (connect_succeed == 1){
    			logged_in_idx = user_index;
    			
				pthread_mutex_lock(&memory_lock);
            	strcpy(users_array[logged_in_idx].ip, client_ip_string);
            	pthread_mutex_unlock(&memory_lock);

    			printf("User %s logged in successfully!\n", temp_id);
    			write(conversation_fd, "HELLO+++\n", 9);
    		}else {
    			printf("Login failed for %s. Wrong ID or Password.\n", temp_id);
    			write(conversation_fd, "GOBYE+++\n", 9);
        		break;
    		}
    			
    	}else if (strncmp (buff,"FRIE?",5) == 0){
    		// FRIE?-----------------------------------------
    			
    		if (logged_in_idx == -1) {
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
            }
    					
    		char target_id [9];
    		int items_found = sscanf(buff, "FRIE? %8[^+]",target_id);
    		if (items_found != 1) {
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
        	}
    				
    		int target_idx = get_user_index(target_id);
    					
    		if ( target_idx == -1 ){
    			printf("Target doesn't exist\n");
    			write(conversation_fd, "FRIE<+++\n", 9);
    					
    		}else {
    			
				printf("%s sent a friend request to %s.\n", users_array[logged_in_idx].user_id, target_id);

				add_flusso_to_user(target_idx, FLUSSO_FRIEND_REQ, users_array[logged_in_idx].user_id, NULL);
    			write(conversation_fd, "FRIE>+++\n", 9);
    					
    			// notifca di udp
				send_udp(target_idx, FLUSSO_FRIEND_REQ);
    		}
    				
    	} else if (strncmp(buff, "CONSU", 5) == 0) {
    		// consu---------------------------------------
    				
    		if (logged_in_idx == -1) {
        		write(conversation_fd, "GOBYE+++\n", 9);
        		break;
    		}
    				
    		// if the linked list is empty : 
    		if (users_array[logged_in_idx].flussoBox_head == NULL) {
        		write(conversation_fd, "NOCON+++\n", 9);
    			
    		}else{
    					
    			struct flusso *current_flusso = users_array[logged_in_idx].flussoBox_head;
    					
    			// if it's a friend request
    			if (current_flusso->type == FLUSSO_FRIEND_REQ){
            		
					printf("%s is reading a friend request from %s.\n", users_array[logged_in_idx].user_id, current_flusso->sender_id);

					char response[64];
            		sprintf(response, "EIRF> %s+++\n", current_flusso->sender_id);
            		write(conversation_fd, response, strlen(response));
            				
            		// NEW LINE: The Waiter memorizes the name!
            		strcpy(pending_friend_id, current_flusso->sender_id);
        				
				}
				// handle friend acceptance
				else if (current_flusso->type == FLUSSO_FRIEND_ACC) {
                    char response[64];
                    sprintf(response, "FRIEN %s+++\n", current_flusso->sender_id);
                    write(conversation_fd, response, strlen(response));
                } 
                // handle friend rejection
                else if (current_flusso->type == FLUSSO_FRIEND_REJ) {
                    char response[64];
                    sprintf(response, "NOFRI %s+++\n", current_flusso->sender_id);
                    write(conversation_fd, response, strlen(response));
                }
				// handle  message flusso
				else if (current_flusso->type == FLUSSO_MESSAGE){
					char response[300];
					sprintf(response, "SSEM> %s %s+++\n", current_flusso->sender_id, current_flusso->text);
                    write(conversation_fd, response, strlen(response));
				}
				// handle flood flusso
				else if (current_flusso->type == FLUSSO_FLOOD){ 
    				char response[300];
    				sprintf(response, "OOLF> %s %s+++\n", current_flusso->sender_id, current_flusso->text);
    				write(conversation_fd, response, strlen(response));
				}else {
                    printf("flusso type %d is corrupted or unknown!\n", current_flusso->type);
                    write(conversation_fd, "NOCON+++\n", 9); // Send something so the client doesn't freeze!
                }
        		
				// lock 
				pthread_mutex_lock(&memory_lock);

				users_array[logged_in_idx].flussoBox_head = current_flusso->next;

        		// If the list is now completely empty, fix the Tail pointer too
        		if (users_array[logged_in_idx].flussoBox_head == NULL) {
            		users_array[logged_in_idx].flussoBox_tail = NULL;
        		}
        				
        				
        		// 7. Update the unread counter and delete the old box from RAM
        		users_array[logged_in_idx].unread_count--;

				// unlock 
				pthread_mutex_unlock(&memory_lock);

        		free(current_flusso);
        				
        		// rest of CONSU
			}		
    	} else if (strncmp(buff, "OKIRF", 5) == 0) {
                // --- OKIRF (Accept Request) ---
                if (logged_in_idx == -1 || strlen(pending_friend_id) == 0) {
                    write(conversation_fd, "GOBYE+++\n", 9);
                   	break;
                }
                
                // Look up the SENDER in our array
                int sender_idx = get_user_index(pending_friend_id);
                
                if (sender_idx != -1) {
                    
					// lock for the 2d array
					pthread_mutex_lock(&memory_lock);
					
					// UPDATE THE MATRIX! They are officially friends! (1 = friends)
                    friends[logged_in_idx][sender_idx] = 1;
                    friends[sender_idx][logged_in_idx] = 1;
					
					// unllock
					pthread_mutex_unlock(&memory_lock);

					printf("%s and %s are now officially friends.\n", users_array[logged_in_idx].user_id, pending_friend_id);
                    
                    write(conversation_fd, "ACKRF+++\n", 8);
							
					add_flusso_to_user(sender_idx, FLUSSO_FRIEND_ACC, users_array[logged_in_idx].user_id, NULL);
                    
					// notifica di udp
					send_udp(sender_idx, FLUSSO_FRIEND_ACC);
            	}
                
                // Clear the notepad so they can't accept twice!
                memset(pending_friend_id, 0, sizeof(pending_friend_id));

        } else if (strncmp(buff, "NOKRF", 5) == 0) {
                // --- NOKRF (Reject Request) ---
                if (logged_in_idx == -1 || strlen(pending_friend_id) == 0) {
                	write(conversation_fd, "GOBYE+++\n", 9);
                    break;
                }
                
                // Look up the SENDER in our array
    			int sender_idx = get_user_index(pending_friend_id);
                
                if (sender_idx != -1) {

					printf("%s declined the friend request from %s.\n", users_array[logged_in_idx].user_id, pending_friend_id);

                    write(conversation_fd, "ACKRF+++\n", 8);
                    
                    add_flusso_to_user(sender_idx, FLUSSO_FRIEND_REJ, users_array[logged_in_idx].user_id, NULL);
                    			
                    // notifica udp 
					send_udp(sender_idx, FLUSSO_FRIEND_REJ);
                }			
                
            	// Clear the notepad
                memset(pending_friend_id, 0, sizeof(pending_friend_id));
                
        } else if (strncmp(buff, "MESS?", 5) == 0){
			// ------ MESS----------------------------------
			if (logged_in_idx == -1 ) {
                    write(conversation_fd, "GOBYE+++\n", 9);
                    break;
            }

			char target_id [9];
			char mess_block[201];
			int stop = 0;

    		int items_found = sscanf(buff, "MESS? %8s %n",target_id,&stop);
    		if (items_found != 1) {
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
            }

			int text_length = terminator - (buff + stop);

			// 3. Security check: make sure it's not too big! [cite: 48]
            if (text_length <= 0 || text_length > 200) {
                write(conversation_fd, "MESS<+++\n", 9); 
                continue;

			}
                    
			// 4. Copy the exact text into our variable!
            memset(mess_block, 0, sizeof(mess_block));
            memcpy(mess_block, &buff[stop], text_length);
			mess_block[text_length] = '\0';
					

    		int target_idx = get_user_index(target_id);
    					
    		if ( target_idx == -1 ){
    			printf("Message failed: Target '%s' doesn't exist\n", target_id);
    			write(conversation_fd, "MESS<+++\n", 9);
			}

			else if ( friends[logged_in_idx][target_idx] != 1 ){
				printf("Message failed: You are not friends with %s\n", target_id);
    			write(conversation_fd, "MESS<+++\n", 9);
			}

			else{
				add_flusso_to_user(target_idx, FLUSSO_MESSAGE, users_array[logged_in_idx].user_id, mess_block);

				write(conversation_fd, "MESS>+++\n", 9);
                printf("Message sent from %s to %s\n", users_array[logged_in_idx].user_id, target_id);

				// notifica udp 
				send_udp(target_idx, FLUSSO_MESSAGE);
			}

		}else if (strncmp(buff, "FLOO?", 5) == 0) {
            //-----------------------------FLOO--------------------
            if (logged_in_idx == -1) {
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
            }

            char mess_block[204];
            
            // Reads up to 200 characters, stops if it sees a '+'
            int items_found = sscanf(buff, "FLOO? %200[^+]", mess_block);
            
            if (items_found != 1) {
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
            }
            
            // Instantly reply to sender
            write(conversation_fd, "FLOO>+++\n", 9);
            
            // Kick off the recursion graph search!
            flood_msg(logged_in_idx, mess_block);

        }else if (strncmp(buff, "IQUIT", 5) == 0){
			// --- IQUIT (Disconnect) ---
            write(conversation_fd, "GOBYE+++\n", 9); // Reply to client
            break; // Break the while(1) loop to run close() and terminate the thread

		}else if (strncmp(buff, "LIST?", 5) == 0){
					
			if (logged_in_idx == -1 ) {
                write(conversation_fd, "GOBYE+++\n", 9);
                break;
            }

			char response[2048] = ""; 
            sprintf(response, "RLIST %03d+++\n", total_users); 
            write(conversation_fd, response, strlen(response));
            
            for (int i = 0; i < total_users; i++) {
                char user_line[64]; 
                sprintf(user_line, "LINUM %s+++\n", users_array[i].user_id);
				strcat(response, user_line);
            }

			write(conversation_fd, response, strlen(response));

			}else{
    			write (conversation_fd,"GOBYE+++\n", 9);
    		}
    }
    
    close(conversation_fd);
    return NULL;
}



//----------------------------------------------------------------------------------------------		
int main(int argc, char *argv[]) {

	if (argc < 2) {
        printf("write: ./server (port number)\n");
        return 1;
    }
    
    struct sockaddr_in server_address;
    memset (&server_address, 0, sizeof(server_address));
    
    server_address.sin_family = AF_INET;  // the protocol :IPv4
    server_address.sin_port = htons(atoi(argv[1]));// the port
    // host to network : from ltl to big endians 
    server_address.sin_addr.s_addr = htonl (INADDR_ANY);  // the IP address
    
    int listen_fd = socket (AF_INET,SOCK_STREAM,0);
    
    if ( listen_fd < 0 ){
    	perror ("Error creating the socket");
    	return 1;
    }
    
    int r = bind (listen_fd,(struct sockaddr *) &server_address,sizeof(struct sockaddr_in));
    
    if ( r == -1 ){
    	perror ("Error binding the socket to the address");
    	return 1;
    }
    
    r = listen (listen_fd,10);
    
    if ( r == -1 ){
    	perror ("Error on listening");
    	return 1;
    }else {
    	printf ("Server listening on port %s\n", argv[1]);
    }
    
    while (1){
    	
    	struct sockaddr_in client_addr;
    	socklen_t client_len = sizeof (client_addr);
    	
    	// accept return a socket to hold the chat 
    	int conversation_fd = accept (listen_fd, (struct sockaddr *) &client_addr, &client_len);

		// extract the ip from the struct
		char *client_ip = inet_ntoa(client_addr.sin_addr);
    	
    	if ( conversation_fd >= 0){
    		
    		// allocate in the heap envelope give it a sizeof int
    		// and i give this FD to the thread and thread will free the memory and that is done so every client will have his own FD in new place in the heap
    		struct client_data *envelope = malloc(sizeof(struct client_data));
            envelope->fd = conversation_fd;
            
            // Extract the real IP from the connection packet and copy it safely
            char *client_ip = inet_ntoa(client_addr.sin_addr);
            strcpy(envelope->ip_str, client_ip);
            
            // Hire the thread and hand it the envelope!
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, handle_client, envelope);
            
            // when finish with the thread clean it up 
            pthread_detach(thread_id); 
    			
    	}
    }
   
    return 0;
}	

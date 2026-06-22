#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

// udp thread starting function
void *udp_ear_thread(void *arg){

    int port = *(int *)arg;  // the port from the main

    // create a UDP socket 
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) return NULL;

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    int r = bind (udp_sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (r < 0){  // check if bind is done 
        printf("\n[Error] Could not bind UDP port %d. Is it already in use?\n> ", port);
        return NULL;
    }

    char udp_buff[10];

    while (1){
        
        memset(udp_buff, 0, sizeof(udp_buff));

        // wait untill packet arrives 
        int bytes = recv(udp_sock, udp_buff, sizeof(udp_buff)-1, 0);

        if (bytes >= 3) {

            // Extract the data: Type (1 byte) + Hex Count (2 bytes)
            char type = udp_buff[0];
            char hex_count[3] = {udp_buff[1], udp_buff[2], '\0'};

            // Convert Hex string back to a normal integer
            // (int) type cast
            int unread = (int)strtol(hex_count, NULL, 16);  // string to long , can't use atoi() because it's 10 base 
             

            printf("\n\n[ UDP notification ]  Type: %c | Unread Messages: %d\n", type, unread);

        }    
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    
    if (argc < 3) {
        printf("write: ./client (server_port) (server_ip)\n");
        return 1;
    }

    // TCP socket
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    if (server_fd == -1){
        perror ("failed to create the socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    inet_pton(AF_INET, argv[2], &server_addr.sin_addr);

    
    // connect to the server's
    int r = connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (r == -1) {
        perror("Failed to connect to server");
        close(server_fd);
        return 1;
    }

    printf("====== Connected to the server! ========\n");
    printf("========================================\n");
    printf("COMMANDS : \n");
    printf(" Register:   REGIS <id> <port> <password>+++\n");
    printf(" Login:      CONNE <id> <password>+++\n");
    printf(" Add Friend: FRIE? <target_id>+++\n");
    printf(" Accept Friend: OKIRF+++\n");
    printf(" Reject Friend: NOKRF+++\n");
    printf(" Message:    MESS? <target_id> <text>+++\n");
    printf(" Flood:      FLOO? <text>+++\n");
    printf(" Read Mail:  CONSU+++\n");
    printf(" User List:  LIST?+++\n");
    printf(" Disconnect: IQUIT+++\n\n");

    char buff[512];
    
    // for the udp thread 
    int my_udp_port = 0;
    bool udp_is_running = false; 
    pthread_t udp_thread;


    while (1) {
        memset(buff, 0, sizeof(buff));

        // fgets reads from keyboard
        if (fgets(buff, sizeof(buff), stdin) == NULL) break;

        // fgets saves the enter from the keyboard as \n, to fix it:
        int n = strcspn(buff, "\n"); // Count characters until you hit \n
        buff[n] = '\0';

        // regis and connect : handling the pass 
        
        if (strncmp(buff, "REGIS", 5) == 0) {
            // Using size 20 to prevent overflow!
            char temp_id[20], temp_port[20], temp_pass[20];
            
            // Read up until the '+' sign
            int items = sscanf(buff, "REGIS %19s %19s %19[^+]", temp_id, temp_port, temp_pass);
            
            if (items >= 3) {
                
                int check_port = atoi(temp_port);
                int check_pass = atoi(temp_pass);

                if (check_port < 1024 || check_port > 9999) {
                    printf("Client Error: Please use a port between 1024 and 9999.\n");
                    continue;
                }

                if (check_pass < 0 || check_pass > 65535) {
                    printf("Client Error: Password must be between 0 and 65535.\n");
                    continue; 
                }
                
                // the UDP Thread
                if (udp_is_running == false) {
                    my_udp_port = atoi(temp_port);
                    pthread_create(&udp_thread, NULL, udp_ear_thread, &my_udp_port);
                    udp_is_running = true;
                }
                
                // 2. Build the precise Binary Packet
                char send_buff[512];
                int len = sprintf(send_buff, "REGIS %s %s ", temp_id, temp_port); // Notice the space at the end!
                
                // 3. Convert password to 2-byte binary and safely copy it
                uint16_t bin_pass = (uint16_t)atoi(temp_pass);
                memcpy(&send_buff[len], &bin_pass, 2);
                len += 2; // Advance the length counter by 2 bytes
                
                // 4. Glue the terminator to the very end
                memcpy(&send_buff[len], "+++", 3);
                len += 3;
                
               
                // 5. Send it using our exact length counter!
                write(server_fd, send_buff, len);
            } else {
                // If they typed it wrong, send it as text so the server rejects it
                write(server_fd, buff, strlen(buff));
            }

        } else if (strncmp(buff, "CONNE", 5) == 0) {
            char temp_id[20], temp_pass[20];
            int items = sscanf(buff, "CONNE %19s %19[^+]", temp_id, temp_pass);
            
            if (items >= 2) {
                
                
                // 2. Build the precise Binary Packet
                char send_buff[512];
                int len = sprintf(send_buff, "CONNE %s ", temp_id); // Notice the space at the end!
                
                // 3. Convert password to 2-byte binary and safely copy it
                uint16_t bin_pass = (uint16_t)atoi(temp_pass);
                memcpy(&send_buff[len], &bin_pass, 2);
                len += 2;
                
                // 4. Glue the terminator to the very end
                memcpy(&send_buff[len], "+++", 3);
                len += 3;
                
                // 5. Send it using our exact length counter!
                write(server_fd, send_buff, len);
            } else {
                 // If item aren't 3 just pass it to server it will handle it 
                write(server_fd, buff, strlen(buff));
            }
            
        } else {
            // FOR EVERYTHING ELSE (MESS?, FRIE?, LIST?, IQUIT+++)
            write(server_fd, buff, strlen(buff));
        }

        // WAIT FOR SERVER REPLY
        
        memset(buff, 0, sizeof(buff));
        int bytes_read = read(server_fd, buff, sizeof(buff) - 1);
        
        if (bytes_read <= 0) {
            printf("\nServer disconnected.\n");
            break;
        }

        // Print what the server said!
        printf("SERVER : %s\n", buff);

        // ask for the port num if he had them right
        if (strncmp(buff, "HELLO+++", 8) == 0 && udp_is_running == false) {
            printf("Enter your UDP Port for notifications:\n");
            char port_input[20];
            fgets(port_input, sizeof(port_input), stdin);
            my_udp_port = atoi(port_input);
            pthread_create(&udp_thread, NULL, udp_ear_thread, &my_udp_port);
            udp_is_running = true;

            printf("\nlogin is done successfully, ready for the commands\n");
        }
        
        // If the server said GOBYE+++, we should close the app
        if (strncmp(buff, "GOBYE+++", 8) == 0) {
            break;
        }
    }

    close(server_fd);

    if (udp_is_running == true) {
        // If the Ear thread is running, safely murder it so it doesn't run forever
        pthread_cancel(udp_thread); 
    }
        
    return 0;   
}
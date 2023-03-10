#include <netinet/in.h> 
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <array>
#include <map>
#include <unordered_map>
#include <bits/stdc++.h>

#define MAX_LIMIT 4096
#define DATA_BUFFER 5000
#define MAX_CONNECTIONS 10 

int close_socket(int fd) {
    close(fd);
    return -1;
}

void broadcast_msg(const char msg[], int src_fd, int all_connections[], int max_conns, fd_set read_fd_set) {
    for (int i=1; i < max_conns; i++) {
        if (all_connections[i] != src_fd && 
            all_connections[i] > 0) {
            send(all_connections[i], msg, std::strlen(msg), 0);
        }
    }
}

int create_tcp_server_socket() {
    struct sockaddr_in saddr;
    int fd, ret_val;
    const int opt = 1;

    /* Step1: create a TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        printf("Could not create socket");
        return -1;
    }

    printf("Created a socket with fd: %d\n", fd);

    /* Step2: set socket options */
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == -1) {  
        printf("Could not set socket options");
        close_socket(fd);
    } 

    /* Initialize the socket address structure */
    saddr.sin_family = AF_INET;         
    saddr.sin_port = htons(5001);     
    saddr.sin_addr.s_addr = INADDR_ANY; 

    /* Step3: bind the socket to port 5001 on the local host */
    if ((ret_val = bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in))) == -1) {
        printf("Could not bind to socket");
        close_socket(fd);
    }

    /* Step4: listen for incoming connections */
    if ((ret_val = listen(fd, 5)) == -1) {
        printf("Could not listen on socket");
        close_socket(fd);
    }

    return fd;
}

int main () {
    /* File descriptor set for reading*/
    fd_set read_fd_set;

    struct sockaddr_in new_addr;
    int addrlen = sizeof(struct sockaddr_in);

    int server_fd, new_fd, ret_val, i, ret_data;
    
    char buf[DATA_BUFFER];
    int all_connections[MAX_CONNECTIONS];

    /* Get the socket server fd */
    server_fd = create_tcp_server_socket(); 

    if (server_fd == -1) {
        printf("Could not create socket");
        return -1; 
    }   

    /* Initialize all_connections and set the first entry to server_fd */
    for (i=0;i < MAX_CONNECTIONS; i++) {
        all_connections[i] = -1;
    }
    
    all_connections[0] = server_fd;

    while (1) {
        /* Clean up all file descriptor set bit flags */
        FD_ZERO(&read_fd_set);

        /* Set the fd_set before passing it to the select call */
        for (i=0;i < MAX_CONNECTIONS;i++) {
            if (all_connections[i] >= 0) {
                /* Set the all_connections[i] th bit in read_fd_set */
                FD_SET(all_connections[i], &read_fd_set);
            }
        }

        /* Invoke select() and then wait! */
        printf("\nUsing select() to listen for incoming events\n");

        /* select() woke up. Identify the fd that has events */
        ret_val = select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);

        if (ret_val != -1) {
            printf("Select returned with %d\n", ret_val);

            /* Check if the fd with event is the server_fd, i.e. this is a new connection
               and not data sent from existing connection */

            if (FD_ISSET(server_fd, &read_fd_set)) { 
                /* accept the new connection */
                printf("Returned fd is %d (server's fd)\n", server_fd);
                new_fd = accept(server_fd, (struct sockaddr*)&new_addr, (socklen_t*)&addrlen);
                
                if (new_fd != -1) {
                    printf("Accepted a new connection with fd: %d\n", new_fd);

                    for (i=0;i < MAX_CONNECTIONS;i++) {
                        if (all_connections[i] < 0) {
                            /* The 1st non-set index in all_connections 
                            is set with new socket file descriptor */
                            all_connections[i] = new_fd; 
                            break;
                        }
                    }
                } 
                
                else {
                    printf("Could not accept connection");
                }

                ret_val--;

                if (!ret_val) continue;
            } 

            /* Check if the fd with event is a non-server fd i.e. existing 
            connection is sending data */

            for (i=1;i < MAX_CONNECTIONS;i++) {
                if ((all_connections[i] > 0) &&
                    (FD_ISSET(all_connections[i], &read_fd_set))) {

                    /* read incoming data */   
                    printf("Returned fd is %d [index, i: %d]\n", all_connections[i], i);

                    ret_data = recv(all_connections[i], buf, DATA_BUFFER, 0);

                    if (ret_data == 0) {
                        /* Connection stopped sending data */
                        printf("Closing connection for fd:%d\n", all_connections[i]);
                        close(all_connections[i]);
                        all_connections[i] = -1; /* Connection is now closed */
                    } 
                    if (ret_data != -1) { 
                        printf("Received data (len %d bytes, fd: %d): %s\n", 
                            ret_data, all_connections[i], buf);

                        std::string final_msg;
                        final_msg.assign(buf);
                        final_msg = std::to_string(all_connections[i]) + ":" + final_msg;
                        broadcast_msg(final_msg.c_str(), all_connections[i], all_connections, MAX_CONNECTIONS, read_fd_set);
                    } 
                    if (ret_data == -1) {
                        printf("recv() failed for fd: %d\n", all_connections[i]);
                        break;
                    }
                }

                ret_val--;

                if (!ret_val) continue;
            }
        }
    }

    /* Last step: Close all the sockets */
    for (i=0;i < MAX_CONNECTIONS;i++) {
        if (all_connections[i] > 0) {
            close(all_connections[i]);
        }
    }

    return 0;
}
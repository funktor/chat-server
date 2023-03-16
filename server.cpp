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
#include <deque>
#include <tuple>
#include <unordered_map>
#include <thread>
#include <fcntl.h>

#define MAX_LIMIT 10
#define DATA_BUFFER 10
#define MAX_CONNECTIONS 1010 

int close_socket(int fd) {
    close(fd);
    return -1;
}

std::vector<std::string> split(std::string &s, std::string delimiter) {
    size_t pos = 0;
    std::vector<std::string> parts;

    while ((pos = s.find(delimiter)) != std::string::npos) {
        std::string token = s.substr(0, pos);
        if (token.size() > 0) parts.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    
    return parts;
}

void broadcast_msg(std::unordered_map<int, std::deque<std::string>> &message, std::mutex &m) {
    while (1) {
        {
            std::lock_guard<std::mutex> lock (m);

            for (auto it=message.begin(); it != message.end(); it++) {
                int recpt = it->first;

                while (message.find(recpt) != message.end() && message[recpt].size() > 0) {
                    std::string msg = (it->second).front();
                    const char *msg_chr = msg.c_str();
                    send(recpt, msg_chr, strlen(msg_chr), 0);
                    message[recpt].pop_front();
                }
            }
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
    if ((ret_val = listen(fd, 1000)) == -1) {
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
    
    std::vector<int> all_connections;
    std::unordered_map<int, std::deque<std::string>> message;
    std::vector<std::string> connections_msg;
    std::mutex m;

    /* Get the socket server fd */
    server_fd = create_tcp_server_socket(); 
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    if (server_fd == -1) {
        printf("Could not create socket");
        return -1; 
    }   

    std::thread t(broadcast_msg, std::ref(message), std::ref(m));

    all_connections.push_back(server_fd);
    connections_msg.push_back("");

    while (1) {
        /* Clean up all file descriptor set bit flags */
        FD_ZERO(&read_fd_set);

        /* Set the fd_set before passing it to the select call */
        for (i=0; i < all_connections.size(); i++) {
            FD_SET(all_connections[i], &read_fd_set);
        }

        /* Invoke select() and then wait! */
        // printf("\nUsing select() to listen for incoming events\n");

        /* select() woke up. Identify the fd that has events */
        ret_val = select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);        

        if (ret_val != -1) {
            // printf("Select returned with %d\n", ret_val);

            /* Check if the fd with event is the server_fd, i.e. this is a new connection
               and not data sent from existing connection */

            if (FD_ISSET(server_fd, &read_fd_set)) { 
                /* accept the new connection */
                printf("Returned fd is %d (server's fd)\n", server_fd);
                new_fd = accept(server_fd, (struct sockaddr*)&new_addr, (socklen_t*)&addrlen);
                
                if (new_fd != -1) {
                    printf("Accepted a new connection with fd: %d\n", new_fd);
                    fcntl(new_fd, F_SETFL, O_NONBLOCK);
                    all_connections.push_back(new_fd);
                    connections_msg.push_back("");
                } 
                
                else {
                    printf("Could not accept connection");
                }

                ret_val--;

                // No other socket has any data, continue while loop
                if (!ret_val) continue;
            } 

            /* Check if the fd with event is a non-server fd i.e. existing 
            connection is sending data */

            for (i=1; i < all_connections.size(); i++) {
                if ((all_connections[i] > 0) &&
                    (FD_ISSET(all_connections[i], &read_fd_set))) {

                    /* read incoming data */   
                    // printf("Returned fd is %d [index, i: %d]\n", all_connections[i], i);

                    char buf[DATA_BUFFER];
                    ret_data = recv(all_connections[i], buf, DATA_BUFFER, 0);

                    if (ret_data == 0) {
                        /* Connection stopped sending data */
                        printf("Closing connection for fd:%d\n", all_connections[i]);
                        close(all_connections[i]);
                        all_connections[i] = -1; /* Connection is now closed */
                        connections_msg[i] = "";
                    } 

                    if (ret_data != -1) { 
                        // printf("Received data (len %d bytes, fd: %d): %s\n", 
                        //     ret_data, all_connections[i], buf);

                        std::string msg(buf, buf+ret_data);
                        msg = connections_msg[i] + msg;

                        std::vector<std::string> parts = split(msg, "<EOM>");
                        connections_msg[i] = msg;

                        for (int j = 0; j < parts.size(); j++) {
                            std::string final_msg = std::to_string(all_connections[i]) + ":" + parts[j] + "<EOM>\0";
                            for (int k = 1; k < all_connections.size(); k++) {
                                if (k != i && all_connections[k] > 0) {
                                    {
                                        std::lock_guard<std::mutex> lock (m);
                                        message[all_connections[k]].push_back(final_msg);

                                    }       
                                }
                            }
                        }
                    } 

                    if (ret_data == -1) {
                        printf("recv() failed for fd: %d\n", all_connections[i]);
                        break;
                    }
                }
            }
        }
    }

    /* Last step: Close all the sockets */
    for (i=0;i < all_connections.size();i++) {
        if (all_connections[i] > 0) {
            close(all_connections[i]);
        }
    }

    t.join();

    return 0;
}
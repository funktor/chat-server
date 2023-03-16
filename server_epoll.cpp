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
#include <sys/epoll.h>

#define MAX_LIMIT 10
#define DATA_BUFFER 10
#define MAX_EVENTS 1000
#define MAX_CONCURRENT_CONNECTIONS 1000

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
    if ((ret_val = listen(fd, MAX_CONCURRENT_CONNECTIONS)) == -1) {
        printf("Could not listen on socket");
        close_socket(fd);
    }

    return fd;
}

int main () {
    struct sockaddr_in new_addr;
    int addrlen = sizeof(struct sockaddr_in);

    /* Store all fds discovered */ 
    std::set<int> all_connections;
    struct epoll_event ev, events[MAX_EVENTS];

    /* Get the socket server fd */
    int server_fd = create_tcp_server_socket(); 

    /* Make the socket non blocking, will not wait for connection indefinitely */ 
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    if (server_fd == -1) {
        perror ("Could not create socket");
        exit(EXIT_FAILURE);
    }   

    /* Create epoll instance */
    int efd = epoll_create1 (0);

    if (efd == -1) {
        perror ("epoll_create");
        exit(EXIT_FAILURE);
    }

    ev.data.fd = server_fd;

    /* Interested in read's events using edge triggered mode */
    ev.events = EPOLLIN | EPOLLET;
    
    /* Allow epoll to monitor the server_fd socket */
    if (epoll_ctl (efd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror ("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    while (1) {
        /* Returns only socket for which there are events */
        int nfds = epoll_wait(efd, events, MAX_EVENTS, -1);

        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        /* Iterate over sockets only having events */
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            /* New connection request received */
            if (fd == server_fd) {
                while (1) {
                    /* Accept new connections */
                    int conn_sock = accept(server_fd, (struct sockaddr*)&new_addr, (socklen_t*)&addrlen);
                    
                    if (conn_sock == -1) {
                        /* We have processed all incoming connections. */
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            break;
                        }
                        else {
                            perror ("accept");
                            break;
                        }
                    }

                    /* Make the new connection non blocking */
                    fcntl(conn_sock, F_SETFL, O_NONBLOCK);

                    /* Monitor new connection for read events in edge triggered mode */
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = conn_sock;
                    all_connections.insert(conn_sock);

                    /* Allow epoll to monitor new connection */
                    if (epoll_ctl(efd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                        perror("epoll_ctl: conn_sock");
                        break;
                    }
                }

                continue;
            } 
            
            else if ((events[i].events & EPOLLERR) || 
                            (events[i].events & EPOLLHUP) || 
                            (!(events[i].events & EPOLLIN))) {
                /* Connection closed */

                if (all_connections.find(fd) != all_connections.end()) {
                    /* Remove from all_connections */
                    all_connections.erase(fd);
                }

                close (fd); // Closing the fd removes from the epoll monitored list
                continue;
            }

            else {
                /* Existing connection sending data */
                std::string remainder = "";

                while (1) {
                    char buf[DATA_BUFFER];
                    /* Bytes received from client */
                    int ret_data = recv(fd, buf, DATA_BUFFER, 0);

                    if (ret_data > 0) {
                        /* Read ret_data number of bytes from buf */
                        std::string msg(buf, buf + ret_data);
                        msg = remainder + msg;

                        /* Parse and split incoming bytes into individual messages */
                        std::vector<std::string> parts = split(msg, "<EOM>");
                        remainder = msg;

                        /* Forward all incoming messages to all other clients */

                        for (int j = 0; j < parts.size(); j++) {
                            std::string final_msg = std::to_string(fd) + ":" + parts[j] + "<EOM>\0";
                            std::cout << final_msg << std::endl;

                            for (auto it = all_connections.begin(); it != all_connections.end(); it++) {
                                if (*it != fd && *it != server_fd) {
                                    const char *msg_chr = final_msg.c_str();
                                    send(*it, msg_chr, strlen(msg_chr), 0);
                                }
                            }
                        }
                    } 
                    else {
                        /* Stopped sending new data */
                        break;
                    }
                }
            }
        }
    }

    return 0;
}
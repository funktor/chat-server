#include <netinet/in.h> 
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <thread>

#define MAX_LIMIT 4096

void recv_messages(int server_fd) {
    int ret_data;

    while (1) {
        char buf[MAX_LIMIT];

        ret_data = recv(server_fd, buf, MAX_LIMIT, 0);
        printf("%s\n", buf);
    }
}

int main () {
    struct sockaddr_in saddr;
    int fd, ret_val, ret;
    struct hostent *local_host; /* need netdb.h for this */
    char msg[MAX_LIMIT];

    /* Step1: create a TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (fd == -1) {
        printf("socket failed \n");
        return -1;
    }
    printf("Created a socket with fd: %d\n", fd);

    /* Let us initialize the server address structure */
    saddr.sin_family = AF_INET;         
    saddr.sin_port = htons(5001);     
    local_host = gethostbyname("127.0.0.1");
    saddr.sin_addr = *((struct in_addr *)local_host->h_addr);

    /* Step2: connect to the TCP server socket */
    ret_val = connect(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret_val == -1) {
        printf("connect failed");
        close(fd);
        return -1;
    }
    printf("The Socket is now connected\n");

    std::thread t(recv_messages, fd);

    while (1) {
        fgets(msg, MAX_LIMIT, stdin);
        ret_val = send(fd, msg, sizeof(msg), 0);
    }

    t.join();

    /* Last step: close the socket */
    close(fd);
    return 0;
}
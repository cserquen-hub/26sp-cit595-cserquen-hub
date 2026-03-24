#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 128

static int parse_hello(const char *msg, int *value) {
    char extra;
    if (sscanf(msg, "HELLO %d%c", value, &extra) == 1) {
        return 0;
    }
    return -1;
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./tcpclient <IP> <PORT> <INITIAL_SEQ>\n");
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int x = atoi(argv[3]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "ERROR invalid IP address\n");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    char send_buf[BUFFER_SIZE];
    memset(send_buf, 0, sizeof(send_buf));
    snprintf(send_buf, sizeof(send_buf), "HELLO %d", x);

    if (send(sockfd, send_buf, strlen(send_buf), 0) < 0) {
        perror("send");
        close(sockfd);
        return 1;
    }

    char recv_buf[BUFFER_SIZE];
    memset(recv_buf, 0, sizeof(recv_buf));

    ssize_t bytes_received = recv(sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(sockfd);
        return 1;
    }
    if (bytes_received == 0) {
        fprintf(stderr, "ERROR server closed connection\n");
        close(sockfd);
        return 1;
    }

    recv_buf[bytes_received] = '\0';
    printf("%s\n", recv_buf);
    fflush(stdout);

    int y;
    if (parse_hello(recv_buf, &y) != 0 || y != x + 1) {
        fprintf(stderr, "ERROR invalid sequence number\n");
        close(sockfd);
        return 1;
    }

    int z = y + 1;
    memset(send_buf, 0, sizeof(send_buf));
    snprintf(send_buf, sizeof(send_buf), "HELLO %d", z);

    if (send(sockfd, send_buf, strlen(send_buf), 0) < 0) {
        perror("send");
        close(sockfd);
        return 1;
    }

    close(sockfd);
    return 0;
}
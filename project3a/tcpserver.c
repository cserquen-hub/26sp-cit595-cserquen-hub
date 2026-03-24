#include <arpa/inet.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 10
#define BUFFER_SIZE 128

static int listen_fd = -1;

static void handle_sigint(int sig) {
    (void)sig;
    if (listen_fd >= 0) {
        close(listen_fd);
    }
    exit(0);
}

static int parse_hello(const char *msg, int *value) {
    char extra;
    if (sscanf(msg, "HELLO %d%c", value, &extra) == 1) {
        return 0;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./tcpserver <PORT>\n");
        return 1;
    }

    int port = atoi(argv[1]);

    signal(SIGINT, handle_sigint);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept");
            continue;
        }

        char recv_buf[BUFFER_SIZE];
        memset(recv_buf, 0, sizeof(recv_buf));

        ssize_t bytes_received = recv(conn_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                perror("recv");
            }
            close(conn_fd);
            continue;
        }

        recv_buf[bytes_received] = '\0';
        printf("%s\n", recv_buf);
        fflush(stdout);

        int x;
        if (parse_hello(recv_buf, &x) != 0) {
            fprintf(stderr, "ERROR invalid message format\n");
            close(conn_fd);
            continue;
        }

        int y = x + 1;
        char send_buf[BUFFER_SIZE];
        memset(send_buf, 0, sizeof(send_buf));
        snprintf(send_buf, sizeof(send_buf), "HELLO %d", y);

        if (send(conn_fd, send_buf, strlen(send_buf), 0) < 0) {
            perror("send");
            close(conn_fd);
            continue;
        }

        memset(recv_buf, 0, sizeof(recv_buf));
        bytes_received = recv(conn_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                perror("recv");
            }
            close(conn_fd);
            continue;
        }

        recv_buf[bytes_received] = '\0';
        printf("%s\n", recv_buf);
        fflush(stdout);

        int z;
        if (parse_hello(recv_buf, &z) != 0 || z != y + 1) {
            fprintf(stderr, "ERROR invalid sequence number\n");
        }

        close(conn_fd);
    }

    return 0;
}
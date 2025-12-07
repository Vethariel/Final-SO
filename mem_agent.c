#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

static void error(const char *msg);
static int tcp_client_init(const char *server_ip, int port);
static void read_meminfo(long *mem_data);

static void error(const char *msg) {
    perror(msg);
    exit(1);
}

static int tcp_client_init(const char *server_ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    if (sock < 0) {
        error("Socket creation error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        error("Invalid address");
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Connection failed");
    }

    return sock;
}

static void read_meminfo(long *mem_data) {
    FILE *fp = fopen("/proc/meminfo", "r");
    char buffer[255];
    long mem_total = 0;
    long mem_free = 0;
    long swap_total = 0;
    long swap_free = 0;

    if (fp == NULL) {
        error("Error opening /proc/meminfo");
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (sscanf(buffer, "MemTotal: %ld kB", &mem_total) == 1) {
            continue;
        } else if (sscanf(buffer, "MemFree: %ld kB", &mem_free) == 1) {
            continue;
        } else if (sscanf(buffer, "SwapTotal: %ld kB", &swap_total) == 1) {
            continue;
        } else if (sscanf(buffer, "SwapFree: %ld kB", &swap_free) == 1) {
            continue;
        }
    }

    fclose(fp);

    mem_data[0] = (mem_total - mem_free) / 1024;
    mem_data[1] = mem_free / 1024;
    mem_data[2] = swap_total / 1024;
    mem_data[3] = swap_free / 1024;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <agent_ip>\n", argv[0]);
        exit(1);
    }

    int socket_fd = tcp_client_init(argv[1], atoi(argv[2]));
    long mem_data[4];

    while (1) {
        read_meminfo(mem_data);

        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "MEM;%s;%ld;%ld;%ld;%ld\n", argv[3], mem_data[0], mem_data[1], mem_data[2], mem_data[3]);

        if (send(socket_fd, message, strlen(message), 0) < 0) {
            error("Send failed");
        }

        if (recv(socket_fd, message, BUFFER_SIZE, 0) < 0) {
            error("Receive failed");
        }

        printf("Server response: %s", message);
        sleep(2);
    }

    close(socket_fd);
    return 0;
}

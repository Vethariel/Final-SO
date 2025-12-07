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
static void read_cpu_totals(unsigned long *cpu_data);
static void compute_cpu_delta(const unsigned long *prev, const unsigned long *curr, float *delta);

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

    printf("Connected to server %s:%d\n", server_ip, port);
    return sock;
}

static void read_cpu_totals(unsigned long *cpu_data) {
    FILE *fp = fopen("/proc/stat", "r");
    char cpu_label[5];
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
    unsigned long steal;

    if (fp == NULL) {
        error("Error opening /proc/stat");
    }

    if (fscanf(fp, "%4s %lu %lu %lu %lu %lu %lu %lu %lu", cpu_label, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 9) {
        cpu_data[0] = user;
        cpu_data[1] = nice;
        cpu_data[2] = system;
        cpu_data[3] = idle;
        cpu_data[4] = iowait;
        cpu_data[5] = irq;
        cpu_data[6] = softirq;
        cpu_data[7] = steal;
    } else {
        fclose(fp);
        error("Error reading CPU info");
    }

    fclose(fp);
}

static void compute_cpu_delta(const unsigned long *prev, const unsigned long *curr, float *delta) {
    float prev_total = prev[0] + prev[1] + prev[2] + prev[3] + prev[4] + prev[5] + prev[6] + prev[7];
    float curr_total = curr[0] + curr[1] + curr[2] + curr[3] + curr[4] + curr[5] + curr[6] + curr[7];
    float total_diff = curr_total - prev_total;
    float idle_diff = curr[3] - prev[3];

    delta[0] = (total_diff - idle_diff) / total_diff * 100.0f;
    delta[1] = (curr[0] - prev[0]) / total_diff * 100.0f;
    delta[2] = (curr[2] - prev[2]) / total_diff * 100.0f;
    delta[3] = idle_diff / total_diff * 100.0f;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <agent_ip>\n", argv[0]);
        exit(1);
    }

    int socket_fd = tcp_client_init(argv[1], atoi(argv[2]));
    unsigned long prev_cpu_data[8];
    unsigned long curr_cpu_data[8];
    float delta[4];

    while (1) {
        read_cpu_totals(prev_cpu_data);
        sleep(1);
        read_cpu_totals(curr_cpu_data);
        compute_cpu_delta(prev_cpu_data, curr_cpu_data, delta);

        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "CPU;%s;%.2f;%.2f;%.2f;%.2f\n", argv[3], delta[0], delta[1], delta[2], delta[3]);

        if (send(socket_fd, message, strlen(message), 0) < 0) {
            error("Send failed");
        }

        if (recv(socket_fd, message, BUFFER_SIZE, 0) < 0) {
            error("Receive failed");
        }

        printf("Server response: %s", message);
        sleep(1);
    }

    close(socket_fd);
    return 0;
}

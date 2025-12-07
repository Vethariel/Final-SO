# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>

#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int TCP_client_init(const char *server_ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation error");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        error("Invalid address/ Address not supported");
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Connection Failed");
    }

    printf("Connected to server %s:%d\n", server_ip, port);

    return sock;
}

void cpu_proc(unsigned long *cpu_data)
{
    FILE *fp;
    char cpu_label[5];
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;

    fp = fopen("/proc/stat", "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        exit(-1);
    }
    if (fscanf(fp, "%4s %lu %lu %lu %lu %lu %lu %lu %lu",
               cpu_label,&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 9)
    {
        cpu_data[0] = user;
        cpu_data[1] = nice;
        cpu_data[2] = system;
        cpu_data[3] = idle;
        cpu_data[4] = iowait;
        cpu_data[5] = irq;
        cpu_data[6] = softirq;
        cpu_data[7] = steal;
    }
    else
    {
        perror("Error reading CPU info");
        fclose(fp);
        exit(-1);
    }
    fclose(fp);
}

void cpu_delta(unsigned long *prev, unsigned long *curr, float *delta)
{
    float prev_cpu_total = (prev[0] + prev[1] + prev[2] + prev[3] + prev[4] + prev[5] + prev[6] + prev[7]);
    float curr_cpu_total = (curr[0] + curr[1] + curr[2] + curr[3] + curr[4] + curr[5] + curr[6] + curr[7]);
    float total_diff = curr_cpu_total - prev_cpu_total;
    float cpu_idle = curr[3] - prev[3];
    delta[0] = (total_diff - cpu_idle) / total_diff * 100.0;
    delta[1] = (curr[0] - prev[0]) / total_diff * 100.0;
    delta[2] = (curr[2] - prev[2]) / total_diff * 100.0;
    delta[3] = cpu_idle / total_diff * 100.0;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <agent_ip>\n", argv[0]);
        exit(1);
    }
    int socket_fd = TCP_client_init(argv[1], atoi(argv[2]));
    unsigned long prev_cpu_data[8];
    unsigned long curr_cpu_data[8];
    float delta[4];

    while(1){
        cpu_proc(prev_cpu_data);
        sleep(1);
        cpu_proc(curr_cpu_data);
        cpu_delta(prev_cpu_data, curr_cpu_data, delta);
        char message[BUFFER_SIZE];

        snprintf(message, BUFFER_SIZE, "CPU;%s;%.2f;%.2f;%.2f;%.2f\n", argv[3], delta[0], delta[1], delta[2], delta[3]);
        if(send(socket_fd, message, strlen(message), 0) < 0) {
            perror("Send failed");
            close(socket_fd);
            exit(-1);
        }
        if(recv(socket_fd, message, BUFFER_SIZE, 0) < 0) {
            perror("Receive failed");
            close(socket_fd);
            exit(-1);
        }

        printf("Server response: %s", message);
        sleep(1);
    }

    close(socket_fd);
    return 0;
}
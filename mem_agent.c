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

    return sock;
}


void mem_proc(long* mem_data) {
    FILE *fp;
    char buffer[255];
    long mem_total = 0, mem_free = 0, swap_total = 0, swap_free = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(-1);
    }
    while(fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (sscanf(buffer, "MemTotal: %d kB", &mem_total) == 1) {
            continue;
        }else if (sscanf(buffer, "MemFree: %d kB", &mem_free) == 1) {
            continue;
        }else if (sscanf(buffer, "SwapTotal: %d kB", &swap_total) == 1) {
            continue;
        }else if (sscanf(buffer, "SwapFree: %d kB", &swap_free) == 1) {
            continue;
        }
    }
    mem_data[0] = (mem_total- mem_free)/1024;
    mem_data[1] = mem_free/1024;
    mem_data[2] = swap_total/1024; 
    mem_data[3] = swap_free/1024;
    fclose(fp);
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <agent_ip>\n", argv[0]);
        exit(1);
    }
    int socket_fd = TCP_client_init(argv[1], atoi(argv[2]));
    long mem_data[4];
    mem_proc(mem_data);

    char message[BUFFER_SIZE];
    snprintf(message, BUFFER_SIZE, "MEM;%s;%ld;%ld;%ld;%ld\n", argv[3], mem_data[0], mem_data[1], mem_data[2], mem_data[3]);
    send(socket_fd, message, strlen(message), 0);
    printf("Message sent to server\n");

    close(socket_fd);
    return 0;
}
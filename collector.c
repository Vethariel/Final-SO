#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define MAX_HOSTS 10

struct host_info {
    char ip[32];
    float cpu_usage;
    float cpu_user;
    float cpu_system;
    float cpu_idle;
    float mem_used;
    float mem_free;
    float swap_total;
    float swap_free;
};

enum agent_type {
    AGENT_UNKNOWN = 0,
    AGENT_CPU,
    AGENT_MEM
};

struct collector {
    int shmid;
    key_t key;
    struct host_info *shared_host_info;
    int client_sock;
    int host_index;
    enum agent_type expected_type;
    char host_ip[32];
};

static void error(const char *msg);
static void tcp_server_init(int port, int *server_sock, struct sockaddr_in *server_addr);
static void *recollector(void *collector_arg);
static bool parse_message(const char *buffer, enum agent_type *type, struct host_info *out);
static int find_or_create_host_slot(struct host_info *hosts, const char *ip);
static void update_cpu_fields(struct host_info *hosts, int idx, const struct host_info *parsed);
static void update_mem_fields(struct host_info *hosts, int idx, const struct host_info *parsed);
static void zero_fields_on_disconnect(struct host_info *hosts, int idx, enum agent_type type);

static void error(const char *msg) {
    perror(msg);
    exit(1);
}

static void tcp_server_init(int port, int *server_sock, struct sockaddr_in *server_addr) {
    int opt = 1;

    *server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_sock == -1) {
        error("Socket failed");
    }

    if (setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error("Setsockopt failed");
    }

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;
    server_addr->sin_port = htons(port);

    if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        error("Bind failed");
    }

    if (listen(*server_sock, 8) < 0) {
        error("Listen failed");
    }

    printf("Server listening on port %d\n", port);
}

static bool parse_message(const char *buffer, enum agent_type *type, struct host_info *out) {
    char type_str[4] = {0};

    if (buffer == NULL || type == NULL || out == NULL) {
        return false;
    }

    if (sscanf(buffer, "%3[^;];", type_str) != 1) {
        printf("Malformed message: %s\n", buffer);
        return false;
    }

    if (strcmp(type_str, "CPU") == 0) {
        *type = AGENT_CPU;
        if (sscanf(buffer, "CPU;%31[^;];%f;%f;%f;%f", out->ip, &out->cpu_usage, &out->cpu_user, &out->cpu_system, &out->cpu_idle) != 5) {
            printf("Failed to parse CPU payload: %s\n", buffer);
            return false;
        }
    } else if (strcmp(type_str, "MEM") == 0) {
        *type = AGENT_MEM;
        if (sscanf(buffer, "MEM;%31[^;];%f;%f;%f;%f", out->ip, &out->mem_used, &out->mem_free, &out->swap_total, &out->swap_free) != 5) {
            printf("Failed to parse MEM payload: %s\n", buffer);
            return false;
        }
    } else {
        printf("Unknown data type: %s\n", buffer);
        return false;
    }

    return true;
}

static int find_or_create_host_slot(struct host_info *hosts, const char *ip) {
    int empty_index = -1;

    if (hosts == NULL || ip == NULL) {
        return -1;
    }

    for (int i = 0; i < MAX_HOSTS; i++) {
        if (hosts[i].ip[0] == '\0' && empty_index == -1) {
            empty_index = i;
        } else if (strncmp(hosts[i].ip, ip, sizeof(hosts[i].ip)) == 0) {
            return i;
        }
    }

    if (empty_index != -1) {
        memset(&hosts[empty_index], 0, sizeof(struct host_info));
        strncpy(hosts[empty_index].ip, ip, sizeof(hosts[empty_index].ip) - 1);
        return empty_index;
    }

    return -1;
}

static void update_cpu_fields(struct host_info *hosts, int idx, const struct host_info *parsed) {
    if (hosts == NULL || parsed == NULL || idx < 0 || idx >= MAX_HOSTS) {
        return;
    }

    strncpy(hosts[idx].ip, parsed->ip, sizeof(hosts[idx].ip) - 1);
    hosts[idx].cpu_usage = parsed->cpu_usage;
    hosts[idx].cpu_user = parsed->cpu_user;
    hosts[idx].cpu_system = parsed->cpu_system;
    hosts[idx].cpu_idle = parsed->cpu_idle;
}

static void update_mem_fields(struct host_info *hosts, int idx, const struct host_info *parsed) {
    if (hosts == NULL || parsed == NULL || idx < 0 || idx >= MAX_HOSTS) {
        return;
    }

    strncpy(hosts[idx].ip, parsed->ip, sizeof(hosts[idx].ip) - 1);
    hosts[idx].mem_used = parsed->mem_used;
    hosts[idx].mem_free = parsed->mem_free;
    hosts[idx].swap_total = parsed->swap_total;
    hosts[idx].swap_free = parsed->swap_free;
}

static void zero_fields_on_disconnect(struct host_info *hosts, int idx, enum agent_type type) {
    if (hosts == NULL || idx < 0 || idx >= MAX_HOSTS) {
        return;
    }

    if (type == AGENT_CPU) {
        hosts[idx].cpu_usage = 0.0f;
        hosts[idx].cpu_user = 0.0f;
        hosts[idx].cpu_system = 0.0f;
        hosts[idx].cpu_idle = 0.0f;
    } else if (type == AGENT_MEM) {
        hosts[idx].mem_used = 0.0f;
        hosts[idx].mem_free = 0.0f;
        hosts[idx].swap_total = 0.0f;
        hosts[idx].swap_free = 0.0f;
    }
}

static void *recollector(void *collector_arg) {
    struct collector *ctx = (struct collector *)collector_arg;
    int client_sock = ctx->client_sock;
    char buffer[BUFFER_SIZE];
    ssize_t read_size;
    struct host_info parsed;

    while ((read_size = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        memset(&parsed, 0, sizeof(parsed));

        enum agent_type msg_type = AGENT_UNKNOWN;
        if (!parse_message(buffer, &msg_type, &parsed)) {
            continue;
        }

        if (ctx->expected_type == AGENT_UNKNOWN) {
            ctx->expected_type = msg_type;
            strncpy(ctx->host_ip, parsed.ip, sizeof(ctx->host_ip) - 1);
        } else if (msg_type != ctx->expected_type) {
            printf("Ignoring mismatched agent type. Expected %d got %d\n", ctx->expected_type, msg_type);
            continue;
        } else if (ctx->host_ip[0] != '\0' && strncmp(ctx->host_ip, parsed.ip, sizeof(ctx->host_ip)) != 0) {
            printf("Ignoring message from different host. Expected %s got %s\n", ctx->host_ip, parsed.ip);
            continue;
        }

        if (ctx->host_index == -1) {
            ctx->host_index = find_or_create_host_slot(ctx->shared_host_info, parsed.ip);
            if (ctx->host_index == -1) {
                printf("No available host slots for %s\n", parsed.ip);
                continue;
            }
        }

        if (ctx->shared_host_info != NULL) {
            if (msg_type == AGENT_CPU) {
                update_cpu_fields(ctx->shared_host_info, ctx->host_index, &parsed);
            } else if (msg_type == AGENT_MEM) {
                update_mem_fields(ctx->shared_host_info, ctx->host_index, &parsed);
            }
        }
        send(client_sock, "Data received\n", 14, 0);
    }

    if (read_size == 0) {
        printf("Client disconnected\n");
    } else if (read_size == -1) {
        perror("Recv failed");
    }

    if (ctx->shared_host_info != NULL && ctx->host_index >= 0) {
        zero_fields_on_disconnect(ctx->shared_host_info, ctx->host_index, ctx->expected_type);
    }

    close(client_sock);
    free(ctx);
    return NULL;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int server_sock;
    int client_sock;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;

    tcp_server_init(atoi(argv[1]), &server_sock, &server_addr);

    key_t key = ftok("collector.c", 'A');
    if (key == -1) {
        error("ftok failed");
    }

    int shmid = shmget(key, sizeof(struct host_info) * MAX_HOSTS, 0666 | IPC_CREAT);
    if (shmid == -1) {
        error("shmget failed");
    }

    struct host_info *shared_hosts_info = (struct host_info *)shmat(shmid, NULL, 0);
    if (shared_hosts_info == (void *)-1) {
        error("shmat failed");
    }
    memset(shared_hosts_info, 0, sizeof(struct host_info) * MAX_HOSTS);

    struct collector collector_data;
    collector_data.shmid = shmid;
    collector_data.key = key;
    collector_data.shared_host_info = shared_hosts_info;
    collector_data.host_index = -1;
    collector_data.expected_type = AGENT_UNKNOWN;
    memset(collector_data.host_ip, 0, sizeof(collector_data.host_ip));

    while (1) {
        client_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        if (client_sock < 0) {
            error("Accept failed");
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        pthread_t client_thread;
        collector_data.client_sock = client_sock;
        collector_data.host_index = -1;
        collector_data.expected_type = AGENT_UNKNOWN;
        memset(collector_data.host_ip, 0, sizeof(collector_data.host_ip));

        struct collector *pclient = malloc(sizeof(struct collector));
        *pclient = collector_data;

        if (pthread_create(&client_thread, NULL, recollector, (void *)pclient) < 0) {
            error("Could not create thread");
            close(client_sock);
            free(pclient);
        }
        pthread_detach(client_thread);
    }

    shmctl(collector_data.shmid, IPC_RMID, NULL);

    close(server_sock);
    return 0;
}

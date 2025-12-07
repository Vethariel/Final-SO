#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void TCP_server_init(int port, int *server_sock, struct sockaddr_in *server_addr)
{
    int opt = 1;

    if ((*server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        error("Socket failed");
    }

    if (setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        error("Setsockopt failed");
    }

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;
    server_addr->sin_port = htons(port);

    if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        error("Bind failed");
    }

    if (listen(*server_sock, 8) < 0)
    {
        error("Listen failed");
    }

    printf("Server listening on port %d\n", port);
}

void *recollector(void *client_socket)
{
    int client_sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE];
    ssize_t read_size;
    while ((read_size = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[read_size] = '\0';
        printf("Received data: %s", buffer);
        send(client_sock, "Data received\n", 14, 0);
    }
    if (read_size == 0)
    {
        printf("Client disconnected\n");
    }
    else if (read_size == -1)
    {
        perror("Recv failed");
    }

    close(client_sock);
    free(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    TCP_server_init(atoi(argv[1]), &server_sock, &server_addr);

    while (1)
    {
        client_len = sizeof(client_addr);
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, (socklen_t *)&client_len)) < 0)
        {
            error("Accept failed");
        }
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        pthread_t client_thread;
        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;

        if (pthread_create(&client_thread, NULL, recollector, (void *)pclient) < 0)
        {
            error("Could not create thread");
            close(client_sock);
            free(pclient);
        }
        pthread_detach(client_thread);
    }

    close(server_sock);
    return 0;
}

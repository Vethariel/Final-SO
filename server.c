# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>

#define PORT 8585
#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}



int TCP_server_init() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error("Socket failed");
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error("Setsockopt failed");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        error("Bind failed");
    }

    if (listen(server_fd, 3) < 0) {
        error("Listen failed");
    }

    printf("Server listening on port %d\n", PORT);

    // Accepting a connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        error("Accept failed");
    }

    return new_socket;
}

int main() {
    int socket_fd = TCP_server_init();
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    // Read data from client
    valread = read(socket_fd, buffer, BUFFER_SIZE);
    printf("Received data: %s\n", buffer);

    // Close the socket
    close(socket_fd);
    return 0;
}
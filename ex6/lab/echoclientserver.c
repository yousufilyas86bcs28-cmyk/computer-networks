[24bcs051@mepcolinux ex6]$cat echoserver.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];
    int port;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[1]);

    /* 1. Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    /* allow quick restart of server on same port */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 2. Prepare server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    /* 3. Bind */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* 4. Listen */
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Echo server listening on port %d...\n", port);

    /* 5. Accept a client */
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    /* 6. Echo loop */
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int n = recv(client_fd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        printf("Received: %s", buffer);

        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Exit command received. Closing connection.\n");
            break;
        }

        /* send the same data back to client */
        send(client_fd, buffer, strlen(buffer), 0);
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
[24bcs051@mepcolinux ex6]$cat echoclient.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 1. Create socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    /* 2. Prepare server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("invalid address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    /* 3. Connect */
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to echo server %s:%s\n", argv[1], argv[2]);
    printf("Type a message ('exit' to quit):\n");

    /* 4. Send/receive loop */
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        if (fgets(buffer, BUF_SIZE, stdin) == NULL) break;

        send(sock_fd, buffer, strlen(buffer), 0);

        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Closing connection.\n");
            break;
        }

        memset(buffer, 0, BUF_SIZE);
        int n = recv(sock_fd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) {
            printf("Server closed connection.\n");
            break;
        }
        printf("Echo from server: %s", buffer);
    }

    close(sock_fd);
    return 0;
}
}
[24bcs051@mepcolinux ex6]$g++  echoserver.c
[24bcs051@mepcolinux ex6]$./a.out 7000
Echo server listening on port 7000...
Client connected: 127.0.0.1:34152
Received: hello world
Received: exit
Exit command received. Closing connection.
[24bcs051@mepcolinux ex6]$./a.out 127.0.0.1 7000
Connected to echo server 127.0.0.1:7000
Type a message ('exit' to quit):
hello world
Echo from server: hello world
exit
Closing connection.



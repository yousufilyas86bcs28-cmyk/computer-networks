[24bcs051@mepcolinux ex8]$cat fullduplexserver.c
/* Full Duplex Chat - Iterative TCP Server (with username/password login)
 *
 * Full duplex: server can SEND and RECEIVE at the same time. select() watches
 * both the keyboard (stdin) and the client socket, so whichever has data is
 * handled immediately - nobody has to wait for their turn.
 *
 * Iterative: one client is served at a time; after it leaves, the server
 * goes back to accept() the next one.
 *
 * Files needed in the same folder:
 *   credentials.txt  -> "username password" per line
 *
 * Compile: gcc full_duplex_server.c -o fd_server
 * Run    : ./fd_server
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 1024
#define MAX_ATTEMPTS 3
#define CRED_FILE "credentials.txt"

/* Check username/password against credentials.txt */
static int check_credentials(const char *user, const char *pass)
{
    FILE *fp = fopen(CRED_FILE, "r");
    char u[64], p[64];
    int ok = 0;

    if (fp == NULL) {
        perror("cannot open credentials file");
        return 0;
    }
    while (fscanf(fp, "%63s %63s", u, p) == 2) {
        if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            ok = 1;
            break;
        }
    }
    fclose(fp);
    return ok;
}

/* Authenticate the connected client; returns 1 on success */
static int authenticate(int connfd, char *username, size_t usize)
{
    char buffer[BUFSIZE], user[64], pass[64];

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        int n = recv(connfd, buffer, BUFSIZE - 1, 0);
        if (n <= 0)
            return 0;
        buffer[n] = '\0';

        if (sscanf(buffer, "%63s %63s", user, pass) == 2 &&
            check_credentials(user, pass)) {
            send(connfd, "AUTH_OK", 7, 0);
            snprintf(username, usize, "%s", user);
            printf("User '%s' logged in.\n", user);
            return 1;
        }
        send(connfd, "AUTH_FAIL", 9, 0);
        printf("Failed login attempt (%d/%d)\n", attempt, MAX_ATTEMPTS);
    }
    return 0;
}

/* Full duplex chat with one client */
static void chat(int connfd, const char *username)
{
    fd_set rfds;
    char buf[BUFSIZE];

    printf("Chat with '%s' started. Type 'bye' to end.\n\n", username);

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);     /* watch keyboard   */
        FD_SET(connfd, &rfds);           /* watch the client */

        if (select(connfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            break;
        }

        /* Something typed on the server keyboard -> send it */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            int n = read(STDIN_FILENO, buf, BUFSIZE - 2);
            if (n <= 0)
                break;
            buf[n] = '\0';
            if (send(connfd, buf, n, 0) < 0)
                break;
            if (strncmp(buf, "bye", 3) == 0) {
                printf("You ended the chat.\n");
                break;
            }
        }

        /* Something arrived from the client -> show it */
        if (FD_ISSET(connfd, &rfds)) {
            int n = recv(connfd, buf, BUFSIZE - 1, 0);
            if (n <= 0) {
                printf("[%s disconnected]\n", username);
                break;
            }
            buf[n] = '\0';
            printf("[%s] %s", username, buf);
            if (buf[n - 1] != '\n')
                printf("\n");
            if (strncmp(buf, "bye", 3) == 0) {
                printf("[%s ended the chat]\n", username);
                break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, opt = 1, port;
    char bind_ip[64] = "", line[64], username[64];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    signal(SIGPIPE, SIG_IGN);

    /* Port and bind IP: from command line (./fd_server <port> [ip])
       or asked at run time */
    if (argc > 1) {
        port = atoi(argv[1]);
        if (argc > 2)
            snprintf(bind_ip, sizeof(bind_ip), "%s", argv[2]);
    } else {
        printf("Enter port to listen on: ");
        if (fgets(line, sizeof(line), stdin) == NULL)
            return 1;
        port = atoi(line);
        printf("Enter IP to bind (press Enter for all interfaces): ");
        if (fgets(bind_ip, sizeof(bind_ip), stdin) == NULL)
            bind_ip[0] = '\0';
        bind_ip[strcspn(bind_ip, "\n")] = '\0';
    }
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);
    if (bind_ip[0] == '\0')
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    else if (inet_pton(AF_INET, bind_ip, &servaddr.sin_addr) != 1) {
        fprintf(stderr, "Invalid bind IP address\n");
        close(listenfd);
        return 1;
    }

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    if (listen(listenfd, 5) < 0) {
        perror("listen failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Full Duplex Chat Server listening on %s:%d...\n",
           bind_ip[0] ? bind_ip : "any IP", port);

    /* ITERATIVE loop: one client fully served before the next is accepted */

    while (1) {
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
        if (connfd < 0) {
            perror("accept failed");
            continue;
        }
        printf("\nClient connected: %s:%d\n",
               inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        if (authenticate(connfd, username, sizeof(username)))
            chat(connfd, username);
        else
            printf("Authentication failed. Closing connection.\n");

        close(connfd);
        printf("Waiting for next client...\n");
    }

    close(listenfd);
    return 0;
}
[24bcs051@mepcolinux ex8]$cat fullduplexclient.c
/* Full Duplex Chat - TCP Client (login with masked password)
 *
 * Full duplex: the client can SEND and RECEIVE at the same time. select()
 * watches both the keyboard (stdin) and the server socket.
 *
 * Compile: gcc full_duplex_client.c -o fd_client
 * Run    : ./fd_client [server_ip]     (default 127.0.0.1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 1024
#define MAX_ATTEMPTS 3

/* Read password and display '*' for every character typed */
static void read_password(char *pw, size_t size)
{
    struct termios oldt, newt;
    size_t i = 0;
    int c;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while ((c = getchar()) != '\n' && c != EOF) {
        if (c == 127 || c == 8) {            /* backspace */
            if (i > 0) {
                i--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (i < size - 1) {
            pw[i++] = (char)c;
            putchar('*');
            fflush(stdout);
        }
    }
    pw[i] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
}

/* Ask for username/password and verify with the server */
static int login(int sockfd)
{
    char user[64], pass[64], msg[BUFSIZE], reply[BUFSIZE];

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf("Username: ");
        if (fgets(user, sizeof(user), stdin) == NULL)
            return 0;
        user[strcspn(user, "\n")] = '\0';

        printf("Password: ");
        fflush(stdout);
        read_password(pass, sizeof(pass));

        snprintf(msg, sizeof(msg), "%s %s", user, pass);
        if (send(sockfd, msg, strlen(msg), 0) < 0) {
            perror("send failed");
            return 0;
        }

        int n = recv(sockfd, reply, BUFSIZE - 1, 0);
        if (n <= 0) {
            printf("Server closed the connection.\n");
            return 0;
        }
        reply[n] = '\0';

        if (strcmp(reply, "AUTH_OK") == 0) {
            printf("Login successful.\n\n");
            return 1;
        }
        printf("Invalid username or password (%d/%d)\n\n",
               attempt, MAX_ATTEMPTS);
    }
    return 0;
}

/* Full duplex chat with the server */
static void chat(int sockfd)
{
    fd_set rfds;
    char buf[BUFSIZE];

    printf("Chat started. Type messages and press Enter. Type 'bye' to quit.\n\n");

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);     /* watch keyboard   */
        FD_SET(sockfd, &rfds);           /* watch the server */

        if (select(sockfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            break;
        }

        /* Something typed on the keyboard -> send it */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            int n = read(STDIN_FILENO, buf, BUFSIZE - 2);
            if (n <= 0)
                break;
            buf[n] = '\0';
            if (send(sockfd, buf, n, 0) < 0)
                break;
            if (strncmp(buf, "bye", 3) == 0) {
                printf("You ended the chat.\n");
                break;
            }
        }

        /* Something arrived from the server -> show it */
        if (FD_ISSET(sockfd, &rfds)) {
            int n = recv(sockfd, buf, BUFSIZE - 1, 0);
            if (n <= 0) {
                printf("[Server disconnected]\n");
                break;
            }
            buf[n] = '\0';
            printf("[Server] %s", buf);
            if (buf[n - 1] != '\n')
                printf("\n");
            if (strncmp(buf, "bye", 3) == 0) {
                printf("[Server ended the chat]\n");
                break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int sockfd, port;
    struct sockaddr_in servaddr;
    char ip[64], line[64];

    /* Server IP and port: from command line (./NAME <ip> <port>)
       or asked at run time */
    if (argc > 2) {
        snprintf(ip, sizeof(ip), "%s", argv[1]);
        port = atoi(argv[2]);
    } else {
        printf("Enter server IP address (press Enter for 127.0.0.1): ");
        if (fgets(ip, sizeof(ip), stdin) == NULL)
            return 1;
        ip[strcspn(ip, "\n")] = '\0';
        if (ip[0] == '\0')
            snprintf(ip, sizeof(ip), "%s", "127.0.0.1");
        printf("Enter server port: ");
        if (fgets(line, sizeof(line), stdin) == NULL)
            return 1;
        port = atoi(line);
    }
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to chat server %s:%d\n\n", ip, port);

    /* User can proceed only after successful login */

    if (!login(sockfd)) {
        printf("Access denied. Exiting.\n");
        close(sockfd);
        return 1;
    }

    chat(sockfd);

    close(sockfd);
    return 0;
}

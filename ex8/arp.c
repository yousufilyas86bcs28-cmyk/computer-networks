[24bcs051@mepcolinux ex8]$cat newarpserver.c
/* ARP Simulation - Iterative TCP Server (with username/password login)
 *
 * Iterative server: serves ONE client at a time. When that client finishes
 * (types "exit" or disconnects), the server goes back to accept() the next one.
 *
 * Files needed in the same folder:
 *   credentials.txt  -> "username password" per line
 *   arp_table.txt    -> "IP-address MAC-address" per line
 *
 * Compile: gcc arp_server.c -o arp_server
 * Run    : ./arp_server
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 1024
#define MAX_ATTEMPTS 3
#define CRED_FILE "credentials.txt"
#define ARP_FILE  "arptable.txt"

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

/* ARP lookup: find MAC for the given IP in arp_table.txt */
static int arp_lookup(const char *ip, char *mac, size_t macsize)
{
    FILE *fp = fopen(ARP_FILE, "r");
    char fip[64], fmac[64];
    int found = 0;

    if (fp == NULL) {
        perror("cannot open ARP table file");
        return 0;
    }
    while (fscanf(fp, "%63s %63s", fip, fmac) == 2) {
        if (strcmp(fip, ip) == 0) {
            snprintf(mac, macsize, "%s", fmac);
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

/* Authenticate the connected client; returns 1 on success */
static int authenticate(int connfd)
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
            printf("User '%s' logged in.\n", user);
            return 1;
        }
        send(connfd, "AUTH_FAIL", 9, 0);
        printf("Failed login attempt (%d/%d)\n", attempt, MAX_ATTEMPTS);
    }
    return 0;
}

/* Serve ARP requests from one authenticated client */
static void serve_client(int connfd)
{
    char buffer[BUFSIZE], reply[BUFSIZE], mac[64];
    struct in_addr tmp;

    while (1) {
        int n = recv(connfd, buffer, BUFSIZE - 1, 0);
        if (n <= 0) {
            printf("Client disconnected.\n");
            break;
        }
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            send(connfd, "Goodbye", 7, 0);
            printf("Client ended the session.\n");
            break;
        }

        printf("ARP request: who has %s ?\n", buffer);

        if (inet_pton(AF_INET, buffer, &tmp) != 1) {
            snprintf(reply, sizeof(reply),
                     "Error: '%s' is not a valid IPv4 address", buffer);
        } else if (arp_lookup(buffer, mac, sizeof(mac))) {
            snprintf(reply, sizeof(reply),
                     "ARP Reply: %s is at %s", buffer, mac);
        } else {
            snprintf(reply, sizeof(reply),
                     "ARP: no entry found for %s", buffer);
        }
        send(connfd, reply, strlen(reply), 0);
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, opt = 1, port;
    char bind_ip[64] = "", line[64];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);

    signal(SIGPIPE, SIG_IGN);      /* don't die if a client vanishes */

    /* Port and bind IP: from command line (./arp_server <port> [ip])
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

    printf("Iterative ARP Server listening on %s:%d...\n",
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

        if (authenticate(connfd))
            serve_client(connfd);
        else
            printf("Authentication failed. Closing connection.\n");

        close(connfd);
        printf("Waiting for next client...\n");
    }

    close(listenfd);
    return 0;
}
[24bcs051@mepcolinux ex8]$cat newarpclient.c
/* ARP Simulation - TCP Client (login with masked password)
 * Compile: gcc arp_client.c -o arp_client
 * Run    : ./arp_client [server_ip]     (default 127.0.0.1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
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

int main(int argc, char *argv[])
{
    int sockfd, port;
    struct sockaddr_in servaddr;
    char buffer[BUFSIZE], reply[BUFSIZE], ip[64], line[64];

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
    printf("Connected to ARP server %s:%d\n\n", ip, port);

    /* User can proceed only after successful login */

    if (!login(sockfd)) {
        printf("Access denied. Exiting.\n");
        close(sockfd);
        return 1;
    }

    printf("Enter an IP address to get its MAC address (type 'exit' to quit).\n\n");

    while (1) {
        printf("Enter IP address: ");
        if (fgets(buffer, BUFSIZE, stdin) == NULL)
            break;
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0)
            continue;

        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            perror("send failed");
            break;
        }

        int n = recv(sockfd, reply, BUFSIZE - 1, 0);
        if (n <= 0) {
            printf("Server closed the connection.\n");
            break;
        }
        reply[n] = '\0';
        printf("Server: %s\n\n", reply);

        if (strcmp(buffer, "exit") == 0)
            break;
    }

    close(sockfd);
    return 0;
}
[24bcs051@mepcolinux ex8]$cat arptable.txt
192.168.1.1 AA:BB:CC:DD:EE:01
192.168.1.10 AA:BB:CC:DD:EE:0A
192.168.1.20 AA:BB:CC:DD:EE:14
192.168.1.30 AA:BB:CC:DD:EE:1E
[24bcs051@mepcolinux ex8]$

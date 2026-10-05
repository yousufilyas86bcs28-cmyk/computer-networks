[24bcs051@mepcolinux ex7]$cat newhalfclient.c
/* Half Duplex Chat - UDP Client (with login; password shown as *****)
 * Communication is turn based: client SENDS first, then RECEIVES, and so on.
 *
 * Compile: gcc half_duplex_client.c -o hd_client
 * Run    : ./hd_client [server_ip]     (default 127.0.0.1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFSIZE 1024
#define MAX_ATTEMPTS 3

/* NEW: read password and display '*' for every character typed */
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

/* NEW: ask for username/password and verify with the server */
static int login(int sockfd, struct sockaddr_in *servaddr)
{
    char user[64], pass[64], msg[BUFSIZE], reply[BUFSIZE];
    socklen_t len = sizeof(*servaddr);

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf("Username: ");
        if (fgets(user, sizeof(user), stdin) == NULL)
            return 0;
        user[strcspn(user, "\n")] = '\0';

        printf("Password: ");
        fflush(stdout);
        read_password(pass, sizeof(pass));

        snprintf(msg, sizeof(msg), "%s %s", user, pass);
        if (sendto(sockfd, msg, strlen(msg), 0,
                   (struct sockaddr *)servaddr, len) < 0) {
            perror("sendto failed");
            return 0;
        }

        int n = recvfrom(sockfd, reply, BUFSIZE - 1, 0,
                         (struct sockaddr *)servaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
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
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[BUFSIZE];
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    socklen_t len = sizeof(servaddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Half Duplex Client (server: %s:%d)\n\n", server_ip, PORT);

    /* NEW: user can proceed only after successful login */
    if (!login(sockfd, &servaddr)) {
        printf("Access denied. Exiting.\n");
        close(sockfd);
        return 1;
    }

    printf("Type 'bye' to end the chat.\n\n");

    while (1) {
        /* ---- Turn 1: client transmits (server is listening) ---- */
        printf("Client : ");
        if (fgets(buffer, BUFSIZE, stdin) == NULL)
            break;
        buffer[strcspn(buffer, "\n")] = '\0';

        if (sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&servaddr, len) < 0) {
            perror("sendto failed");
            break;
        }

        if (strcmp(buffer, "bye") == 0) {
            printf("Chat ended.\n");
            break;
        }

        /* ---- Turn 2: client listens (server is transmitting) ---- */
        printf("[Waiting for server...]\n");
        int n = recvfrom(sockfd, buffer, BUFSIZE - 1, 0,
                         (struct sockaddr *)&servaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            break;
        }
        buffer[n] = '\0';
        printf("Server : %s\n", buffer);

        if (strcmp(buffer, "bye") == 0) {
            printf("Server ended the chat.\n");
            break;
        }
    }

    close(sockfd);
    return 0;
[24bcs051@mepcolinux ex7]$cat newhalfserver.c
/* Half Duplex Chat - UDP Server (with username/password authentication)
 * Credentials are read from "credentials.txt" (format: username password)
 * Communication is turn based: server RECEIVES first, then SENDS, and so on.
 *
 * Compile: gcc half_duplex_server.c -o hd_server
 * Run    : ./hd_server        (credentials.txt must be in the same folder)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFSIZE 1024
#define CRED_FILE "credentials.txt"

/* NEW: check username/password against the text file */
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

int main(void)
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr, authaddr;
    char buffer[BUFSIZE], reply[BUFSIZE];
    socklen_t len = sizeof(cliaddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    memset(&authaddr, 0, sizeof(authaddr));

    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Half Duplex Server ready on port %d\n", PORT);

    /* ---------- NEW: authentication phase ---------- */
    int authenticated = 0;
    while (!authenticated) {
        char user[64], pass[64];
        int n = recvfrom(sockfd, buffer, BUFSIZE - 1, 0,
                         (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }
        buffer[n] = '\0';

        if (sscanf(buffer, "%63s %63s", user, pass) == 2 &&
            check_credentials(user, pass)) {
            strcpy(reply, "AUTH_OK");
            authenticated = 1;
            authaddr = cliaddr;
            printf("User '%s' logged in from %s:%d\n", user,
                   inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        } else {
            strcpy(reply, "AUTH_FAIL");
            printf("Failed login attempt from %s:%d\n",
                   inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        }
        sendto(sockfd, reply, strlen(reply), 0,
               (struct sockaddr *)&cliaddr, len);
    }

    printf("Type 'bye' to end the chat.\n\n");

  
    while (1) {
        /* ---- Turn 1: server listens (client is transmitting) ---- */
        printf("[Waiting for client...]\n");
        int n = recvfrom(sockfd, buffer, BUFSIZE - 1, 0,
                         (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            break;
        }
        buffer[n] = '\0';

       
        if (cliaddr.sin_addr.s_addr != authaddr.sin_addr.s_addr ||
            cliaddr.sin_port != authaddr.sin_port) {
            strcpy(reply, "Error: login required");
            sendto(sockfd, reply, strlen(reply), 0,
                   (struct sockaddr *)&cliaddr, len);
            continue;
        }

        printf("Client : %s\n", buffer);

        if (strcmp(buffer, "bye") == 0) {
            printf("Client ended the chat.\n");
            break;
        }

      
        printf("Server : ");
        if (fgets(buffer, BUFSIZE, stdin) == NULL)
            break;
        buffer[strcspn(buffer, "\n")] = '\0';

        sendto(sockfd, buffer, strlen(buffer), 0,
               (struct sockaddr *)&cliaddr, len);

        if (strcmp(buffer, "bye") == 0) {
            printf("Chat ended.\n");
            break;
        }
    }

    close(sockfd);
    return 0;

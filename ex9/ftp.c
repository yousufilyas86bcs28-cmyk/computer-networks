[24bcs051@mepcolinux server_files]$ls
1.txt  a.out  ftpserver.c  server_files  users.txt
[24bcs051@mepcolinux server_files]$cat ftpserver.c
/* FTP Simulation - Concurrent TCP Server (fork per client)
 *
 * Commands supported: LOGIN, LIST (ls), GET, PUT, PWD, MKDIR, DEL, QUIT
 * Files are stored in the folder "server_files" (created automatically).
 *
 * Compile: gcc ftp_server.c -o ftp_server
 * Run    : ./ftp_server
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 2121
#define BUFSIZE 4096
#define MAX_ATTEMPTS 3
#define ROOT_DIR "server_files"

/* user accounts file: one "username password" pair per line */
#define USERS_FILE "users.txt"

/* ---------- helper functions ---------- */

static int send_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0)
            return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_line(int fd, char *buf, size_t size)
{
    size_t i = 0;
    char c;

    while (i < size - 1) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

static int send_msg(int fd, const char *msg)
{
    return send_all(fd, msg, strlen(msg));
}

/* only plain file names allowed (no paths, so clients stay inside ROOT_DIR) */
static int valid_name(const char *name)
{
    if (name[0] == '\0' || strchr(name, '/') != NULL)
        return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;
    return 1;
}

static int check_login(const char *user, const char *pass)
{
    char u[64], p[64];
    int found = 0;
    FILE *fp = fopen(USERS_FILE, "r");

    if (fp == NULL) {
        perror("cannot open " USERS_FILE);
        return 0;
    }
    while (fscanf(fp, "%63s %63s", u, p) == 2) {
        if (strcmp(user, u) == 0 && strcmp(pass, p) == 0) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

/* ---------- command handlers ---------- */

static int do_login(int fd)
{
    char line[BUFSIZE], cmd[16], user[64], pass[64];
    int attempt;

    for (attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (recv_line(fd, line, sizeof(line)) < 0)
            return 0;
        user[0] = pass[0] = '\0';
        if (sscanf(line, "%15s %63s %63s", cmd, user, pass) == 3 &&
            strcmp(cmd, "LOGIN") == 0 && check_login(user, pass)) {
            send_msg(fd, "OK Login successful\n");
            printf("[%d] user '%s' logged in\n", (int)getpid(), user);
            return 1;
        }
        send_msg(fd, "ERR Invalid username or password\n");
    }
    return 0;
}

static void do_list(int fd)
{
    char list[8192], head[64], path[512];
    size_t used = 0;
    DIR *d = opendir(".");
    struct dirent *e;
    struct stat st;

    if (d == NULL) {
        send_msg(fd, "ERR Cannot read directory\n");
        return;
    }
    list[0] = '\0';
    while ((e = readdir(d)) != NULL && used < sizeof(list) - 300) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s", e->d_name);
        if (stat(path, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            used += (size_t)snprintf(list + used, sizeof(list) - used,
                                     "  %s/  [DIR]\n", e->d_name);
        else
            used += (size_t)snprintf(list + used, sizeof(list) - used,
                                     "  %s  (%lld bytes)\n", e->d_name,
                                     (long long)st.st_size);
    }
    closedir(d);

    snprintf(head, sizeof(head), "OK %zu\n", used);
    send_msg(fd, head);
    send_all(fd, list, used);
}

static int do_get(int fd, const char *name)
{
    char buf[BUFSIZE], head[64];
    struct stat st;
    FILE *fp;
    size_t n;

    if (!valid_name(name) || stat(name, &st) != 0 || !S_ISREG(st.st_mode) ||
        (fp = fopen(name, "rb")) == NULL) {
        send_msg(fd, "ERR File not found\n");
        return 0;
    }
    snprintf(head, sizeof(head), "OK %lld\n", (long long)st.st_size);
    if (send_msg(fd, head) < 0) {
        fclose(fp);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(fd, buf, n) < 0) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}

static int do_put(int fd, const char *name, long long size)
{
    char buf[BUFSIZE];
    long long got = 0;
    FILE *fp;

    if (!valid_name(name) || size < 0) {
        send_msg(fd, "ERR Invalid file name\n");
        return 0;
    }
    fp = fopen(name, "wb");
    if (fp == NULL) {
        send_msg(fd, "ERR Cannot create file\n");
        return 0;
    }
    if (send_msg(fd, "OK Ready\n") < 0) {
        fclose(fp);
        return -1;
    }
    while (got < size) {
        size_t want = (size - got) > (long long)sizeof(buf)
                          ? sizeof(buf) : (size_t)(size - got);
        ssize_t n = recv(fd, buf, want, 0);
        if (n <= 0) {                       /* client vanished: drop partial file */
            fclose(fp);
            remove(name);
            return -1;
        }
        fwrite(buf, 1, (size_t)n, fp);
        got += n;
    }
    fclose(fp);
    send_msg(fd, "OK Upload complete\n");
    return 0;
}

static void do_pwd(int fd)
{
    char cwd[512], msg[600];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        send_msg(fd, "ERR Cannot get directory\n");
        return;
    }
    snprintf(msg, sizeof(msg), "OK %s\n", cwd);
    send_msg(fd, msg);
}

static void do_mkdir(int fd, const char *name)
{
    if (!valid_name(name))
        send_msg(fd, "ERR Invalid directory name\n");
    else if (mkdir(name, 0755) == 0)
        send_msg(fd, "OK Directory created\n");
    else if (errno == EEXIST)
        send_msg(fd, "ERR Already exists\n");
    else
        send_msg(fd, "ERR Cannot create directory\n");
}

static void do_del(int fd, const char *name)
{
    struct stat st;

    if (!valid_name(name) || stat(name, &st) != 0)
        send_msg(fd, "ERR No such file or directory\n");
    else if (S_ISDIR(st.st_mode)) {
        if (rmdir(name) == 0)
            send_msg(fd, "OK Directory deleted\n");
        else
            send_msg(fd, "ERR Directory not empty\n");
    } else if (unlink(name) == 0)
        send_msg(fd, "OK File deleted\n");
    else
        send_msg(fd, "ERR Cannot delete file\n");
}

/* ---------- one client session (runs in child process) ---------- */

static void handle_client(int fd)
{
    char line[BUFSIZE], cmd[16], arg[256];
    long long num;

    if (!do_login(fd)) {                /* login first: users.txt is in the start dir */
        printf("[%d] login failed, closing\n", (int)getpid());
        return;
    }
    if (chdir(ROOT_DIR) != 0) {
        perror("chdir");
        return;
    }

    while (recv_line(fd, line, sizeof(line)) >= 0) {
        cmd[0] = arg[0] = '\0';
        num = 0;
        if (sscanf(line, "%15s %255s %lld", cmd, arg, &num) < 1)
            continue;

        if (strcmp(cmd, "LIST") == 0) {
            do_list(fd);
        } else if (strcmp(cmd, "GET") == 0 && arg[0]) {
            if (do_get(fd, arg) < 0) break;
        } else if (strcmp(cmd, "PUT") == 0 && arg[0]) {
            if (do_put(fd, arg, num) < 0) break;
        } else if (strcmp(cmd, "PWD") == 0) {
            do_pwd(fd);
        } else if (strcmp(cmd, "MKDIR") == 0 && arg[0]) {
            do_mkdir(fd, arg);
        } else if (strcmp(cmd, "DEL") == 0 && arg[0]) {
            do_del(fd, arg);
        } else if (strcmp(cmd, "QUIT") == 0) {
            send_msg(fd, "OK Goodbye\n");
            break;
        } else {
            send_msg(fd, "ERR Unknown command\n");
        }
    }
    printf("[%d] client disconnected\n", (int)getpid());
}

int main(void)
{
    int listenfd, connfd, opt = 1;
    pid_t pid;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen;

    setvbuf(stdout, NULL, _IOLBF, 0);   /* line-buffered: no duplicate output after fork */
    signal(SIGCHLD, SIG_IGN);   /* no zombie child processes */
    signal(SIGPIPE, SIG_IGN);   /* don't die if a client disconnects mid-send */

    mkdir(ROOT_DIR, 0755);      /* ignore error if it already exists */

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(PORT);

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
    printf("FTP server listening on port %d (files in '%s/')\n", PORT, ROOT_DIR);

    while (1) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept failed");
            continue;
        }
        printf("Connection from %s:%d\n",
               inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(connfd);
        } else if (pid == 0) {          /* child: serve this client */
            close(listenfd);
            handle_client(connfd);
            close(connfd);
            exit(0);
        } else {                        /* parent: go back to accept() */
            close(connfd);
        }
    }
    return 0;
}
[24bcs051@mepcolinux server_files]$cat users.txt
admin admin123
yousuf 1234
guest guest
[24bcs051@mepcolinux server_files]$cd ..
[24bcs051@mepcolinux exx9new]$cd client
[24bcs051@mepcolinux client]$ls
a.out  ftpclient.c  testcase.txt
[24bcs051@mepcolinux client]$cat ftpclient.c
/* FTP Simulation - TCP Client (login with masked password)
 *
 * Commands at the "ftp>" prompt:
 *   ls                 list files on the server
 *   pwd                show server's current directory
 *   mkdir <name>       create a directory on the server
 *   del <name>         delete a file / empty directory on the server
 *   get <filename>     download a file from the server
 *   put <filename>     upload a file to the server
 *   help               show commands
 *   quit               disconnect
 *
 * Compile: gcc ftp_client.c -o ftp_client
 * Run    : ./ftp_client [server_ip]     (default 127.0.0.1)
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 2121
#define BUFSIZE 4096
#define MAX_ATTEMPTS 3

/* ---------- helper functions ---------- */

static int send_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0)
            return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_line(int fd, char *buf, size_t size)
{
    size_t i = 0;
    char c;

    while (i < size - 1) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            buf[i++] = c;
    }
    buf[i] = '\0';
    return (int)i;
}

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

/* ask for username/password and verify with the server */
static int login(int sockfd)
{
    char user[64], pass[64], msg[256], reply[BUFSIZE];
    int attempt;                              /* declared here: fixes C89 error */

    for (attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf("Username: ");
        if (fgets(user, sizeof(user), stdin) == NULL)
            return 0;
        user[strcspn(user, "\n")] = '\0';

        printf("Password: ");
        fflush(stdout);
        read_password(pass, sizeof(pass));

        snprintf(msg, sizeof(msg), "LOGIN %s %s\n", user, pass);
        if (send_all(sockfd, msg, strlen(msg)) < 0) {
            perror("send failed");
            return 0;
        }
        if (recv_line(sockfd, reply, sizeof(reply)) < 0) {
            printf("Server closed the connection.\n");
            return 0;
        }
        if (strncmp(reply, "OK", 2) == 0) {
            printf("Login successful.\n\n");
            return 1;
        }
        printf("Invalid username or password (%d/%d)\n\n",
               attempt, MAX_ATTEMPTS);
    }
    return 0;
}

/* ---------- FTP commands ---------- */

static int cmd_ls(int fd)
{
    char line[BUFSIZE], buf[BUFSIZE];
    long long len = 0, got = 0;

    if (send_all(fd, "LIST\n", 5) < 0 || recv_line(fd, line, sizeof(line)) < 0)
        return -1;
    if (strncmp(line, "OK", 2) != 0) {
        printf("Server: %s\n\n", line);
        return 0;
    }
    sscanf(line, "OK %lld", &len);

    printf("\nFiles on server:\n");
    if (len == 0)
        printf("  (empty)\n");
    while (got < len) {
        size_t want = (len - got) > (long long)(sizeof(buf) - 1)
                          ? sizeof(buf) - 1 : (size_t)(len - got);
        ssize_t n = recv(fd, buf, want, 0);
        if (n <= 0)
            return -1;
        buf[n] = '\0';
        printf("%s", buf);
        got += n;
    }
    printf("\n");
    return 0;
}

/* send a one-line command, print the server's one-line reply */
static int simple_cmd(int fd, const char *cmd, const char *arg)
{
    char msg[300], line[BUFSIZE];

    if (arg != NULL)
        snprintf(msg, sizeof(msg), "%s %s\n", cmd, arg);
    else
        snprintf(msg, sizeof(msg), "%s\n", cmd);

    if (send_all(fd, msg, strlen(msg)) < 0 ||
        recv_line(fd, line, sizeof(line)) < 0)
        return -1;

    /* strip the leading "OK " / "ERR " for a cleaner display */
    if (strncmp(line, "OK ", 3) == 0)
        printf("%s\n\n", line + 3);
    else
        printf("Server: %s\n\n", line);
    return 0;
}

static int cmd_get(int fd, const char *name)
{
    char line[BUFSIZE], buf[BUFSIZE], msg[300];
    long long size = 0, got = 0;
    FILE *fp;

    snprintf(msg, sizeof(msg), "GET %s\n", name);
    if (send_all(fd, msg, strlen(msg)) < 0 ||
        recv_line(fd, line, sizeof(line)) < 0)
        return -1;
    if (strncmp(line, "OK", 2) != 0) {
        printf("Server: %s\n\n", line);
        return 0;
    }
    sscanf(line, "OK %lld", &size);

    fp = fopen(name, "wb");
    if (fp == NULL)
        perror("cannot create local file (data will be discarded)");

    while (got < size) {
        size_t want = (size - got) > (long long)sizeof(buf)
                          ? sizeof(buf) : (size_t)(size - got);
        ssize_t n = recv(fd, buf, want, 0);
        if (n <= 0) {
            if (fp)
                fclose(fp);
            return -1;
        }
        if (fp)
            fwrite(buf, 1, (size_t)n, fp);
        got += n;
    }
    if (fp) {
        fclose(fp);
        printf("Downloaded '%s' (%lld bytes)\n\n", name, size);
    }
    return 0;
}

static int cmd_put(int fd, const char *path)
{
    char line[BUFSIZE], buf[BUFSIZE], msg[300];
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    struct stat st;
    FILE *fp;
    size_t n;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) ||
        (fp = fopen(path, "rb")) == NULL) {
        printf("Local file '%s' not found.\n\n", path);
        return 0;
    }

    snprintf(msg, sizeof(msg), "PUT %s %lld\n", name, (long long)st.st_size);
    if (send_all(fd, msg, strlen(msg)) < 0 ||
        recv_line(fd, line, sizeof(line)) < 0) {
        fclose(fp);
        return -1;
    }
    if (strncmp(line, "OK", 2) != 0) {          /* server refused */
        printf("Server: %s\n\n", line);
        fclose(fp);
        return 0;
    }

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(fd, buf, n) < 0) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    if (recv_line(fd, line, sizeof(line)) < 0)
        return -1;
    printf("Server: %s\n\n", line);
    return 0;
}

static void show_help(void)
{
    printf("Commands:\n");
    printf("  ls               list files on the server\n");
    printf("  pwd              show server's current directory\n");
    printf("  mkdir <name>     create a directory on the server\n");
    printf("  del <name>       delete a file or empty directory\n");
    printf("  get <filename>   download a file\n");
    printf("  put <filename>   upload a file\n");
    printf("  help             show this help\n");
    printf("  quit             disconnect\n\n");
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in servaddr;
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    char input[BUFSIZE], cmd[16], arg[256], line[BUFSIZE];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to FTP server %s:%d\n\n", server_ip, PORT);

    /* user can proceed only after successful login */
    if (!login(sockfd)) {
        printf("Access denied. Exiting.\n");
        close(sockfd);
        return 1;
    }

    show_help();

    while (1) {
        int rc = 0;

        printf("ftp> ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        cmd[0] = arg[0] = '\0';
        if (sscanf(input, "%15s %255s", cmd, arg) < 1)
            continue;

        if (strcmp(cmd, "ls") == 0) {
            rc = cmd_ls(sockfd);
        } else if (strcmp(cmd, "pwd") == 0) {
            rc = simple_cmd(sockfd, "PWD", NULL);
        } else if (strcmp(cmd, "mkdir") == 0 && arg[0]) {
            rc = simple_cmd(sockfd, "MKDIR", arg);
        } else if (strcmp(cmd, "del") == 0 && arg[0]) {
            rc = simple_cmd(sockfd, "DEL", arg);
        } else if (strcmp(cmd, "get") == 0 && arg[0]) {
            rc = cmd_get(sockfd, arg);
        } else if (strcmp(cmd, "put") == 0 && arg[0]) {
            rc = cmd_put(sockfd, arg);
        } else if (strcmp(cmd, "help") == 0) {
            show_help();
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            send_all(sockfd, "QUIT\n", 5);
            if (recv_line(sockfd, line, sizeof(line)) >= 0)
                printf("Server: %s\n", line);
            break;
        } else {
            printf("Unknown command or missing filename. Type 'help'.\n\n");
        }

        if (rc < 0) {
            printf("Connection lost.\n");
            break;
        }
    }

    close(sockfd);
    return 0;
}

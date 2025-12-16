#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <getopt.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10
#define BUFFER_SIZE 1024

static int server_fd = -1;
static volatile sig_atomic_t exit_requested = 0;

static void signal_handler(int signo)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_requested = 1;
    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR);
    }
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    int opt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') {
            daemon_mode = 1;
        }
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "socket failed");
        return -1;
    }

    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "bind failed");
        close(server_fd);
        return -1;
    }

    /* Daemonize AFTER bind */
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "fork failed");
            return -1;
        }
        if (pid > 0) {
            exit(0);
        }
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen failed");
        close(server_fd);
        return -1;
    }

    while (!exit_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (exit_requested)
                break;
            syslog(LOG_ERR, "accept failed");
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s",
               inet_ntoa(client_addr.sin_addr));

        FILE *fp = fopen(DATA_FILE, "a+");
        if (!fp) {
            syslog(LOG_ERR, "file open failed");
            close(client_fd);
            continue;
        }

        char buffer[BUFFER_SIZE];
        char *packet = NULL;
        size_t packet_size = 0;
        ssize_t bytes_received;

        while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            char *newline = memchr(buffer, '\n', bytes_received);
            size_t copy_len = newline ? (newline - buffer + 1) : bytes_received;

            char *tmp = realloc(packet, packet_size + copy_len);
            if (!tmp) {
                syslog(LOG_ERR, "realloc failed");
                free(packet);
                packet = NULL;
                break;
            }

            packet = tmp;
            memcpy(packet + packet_size, buffer, copy_len);
            packet_size += copy_len;

            if (newline) {
                fwrite(packet, 1, packet_size, fp);
                fflush(fp);

                fseek(fp, 0, SEEK_SET);
                while ((bytes_received = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                    send(client_fd, buffer, bytes_received, 0);
                }
                break;
            }
        }

        free(packet);
        fclose(fp);
        close(client_fd);

        syslog(LOG_INFO, "Closed connection from %s",
               inet_ntoa(client_addr.sin_addr));
    }

    close(server_fd);
    unlink(DATA_FILE);
    closelog();
    return 0;
}


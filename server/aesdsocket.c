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
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <getopt.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10
#define BUFFER_SIZE 1024

static int server_fd = -1;
static volatile sig_atomic_t exit_requested = 0;
static bool daemon_mode = false;

/* Thread-safe signal handler */
static void signal_handler(int signo)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_requested = 1;
}

/* Helper function to send entire file contents */
static void send_entire_file(int data_fd, int client_fd)
{
    // Go to beginning of file
    lseek(data_fd, 0, SEEK_SET);
    
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    // Read entire file and send it
    while ((bytes_read = read(data_fd, file_buffer, sizeof(file_buffer))) > 0) {
        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t result = send(client_fd, file_buffer + bytes_sent, 
                                  bytes_read - bytes_sent, 0);
            if (result <= 0) {
                if (errno == EINTR) continue;
                return; // Connection issue
            }
            bytes_sent += result;
        }
    }
}

/* Thread function to handle a client */
void* handle_client(void* arg)
{
    int client_fd = *(int*)arg;
    free(arg);
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    
    if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    } else {
        strcpy(client_ip, "unknown");
    }
    
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);
    
    int data_fd = -1;
    char buffer[BUFFER_SIZE];
    char* packet = NULL;
    size_t packet_size = 0;
    ssize_t bytes_received;
    
    data_fd = open(DATA_FILE, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (data_fd == -1) {
        syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
        close(client_fd);
        return NULL;
    }
    
    // Read all data from client
    while (!exit_requested && (bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Null terminate for safety
        
        char* current_pos = buffer;
        size_t remaining = bytes_received;
        
        while (remaining > 0) {
            // Look for newline in current chunk
            char* newline = memchr(current_pos, '\n', remaining);
            size_t chunk_size = newline ? (newline - current_pos + 1) : remaining;
            
            // Append chunk to packet buffer
            char* new_packet = realloc(packet, packet_size + chunk_size + 1);
            if (!new_packet) {
                syslog(LOG_ERR, "Memory allocation failed");
                free(packet);
                packet = NULL;
                packet_size = 0;
                break;
            }
            packet = new_packet;
            memcpy(packet + packet_size, current_pos, chunk_size);
            packet_size += chunk_size;
            packet[packet_size] = '\0';
            
            // If we found a complete packet (ends with newline)
            if (newline) {
                // Write the complete packet to file
                ssize_t written = write(data_fd, packet, packet_size);
                if (written != (ssize_t)packet_size) {
                    syslog(LOG_ERR, "Failed to write to data file: %s", strerror(errno));
                }
                
                // Send entire file contents back to client
                send_entire_file(data_fd, client_fd);
                
                // Reset for next packet
                free(packet);
                packet = NULL;
                packet_size = 0;
            }
            
            current_pos += chunk_size;
            remaining -= chunk_size;
        }
    }
    
    // If connection closed and we have an incomplete packet
    if (packet_size > 0) {
        // Write whatever data we have (even without newline)
        ssize_t written = write(data_fd, packet, packet_size);
        if (written == (ssize_t)packet_size) {
            // Send entire file back
            send_entire_file(data_fd, client_fd);
        }
    }
    
    free(packet);
    
    if (data_fd != -1) {
        close(data_fd);
    }
    
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    close(client_fd);
    
    return NULL;
}

int main(int argc, char *argv[])
{
    int opt;
    
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = true;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    openlog("aesdsocket", LOG_PID, LOG_USER);
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set SIGINT handler: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set SIGTERM handler: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }
    
    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return EXIT_FAILURE;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return EXIT_FAILURE;
    }
    
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return EXIT_FAILURE;
    }
    
    syslog(LOG_INFO, "Server started on port %d", PORT);
    
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "fork failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return EXIT_FAILURE;
        }
        if (pid > 0) {
            close(server_fd);
            closelog();
            exit(EXIT_SUCCESS);
        }
        if (setsid() == -1) {
            syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return EXIT_FAILURE;
        }
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        closelog();
        openlog("aesdsocket", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "Server daemon started on port %d", PORT);
    }
    
    while (!exit_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (exit_requested) break;
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }
        
        int* client_fd_ptr = malloc(sizeof(int));
        if (!client_fd_ptr) {
            syslog(LOG_ERR, "Memory allocation failed");
            close(client_fd);
            continue;
        }
        *client_fd_ptr = client_fd;
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_fd_ptr) != 0) {
            syslog(LOG_ERR, "Thread creation failed");
            free(client_fd_ptr);
            close(client_fd);
            continue;
        }
        
        pthread_detach(thread_id);
    }
    
    syslog(LOG_INFO, "Server shutting down");
    
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
    
    // REMOVE DATA FILE ON EXIT (REQUIRED)
    if (unlink(DATA_FILE) == -1 && errno != ENOENT) {
        syslog(LOG_ERR, "Failed to remove data file: %s", strerror(errno));
    }
    
    closelog();
    
    return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    // Open syslog
    openlog("writer", LOG_PID, LOG_USER);

    // Check arguments
    if (argc != 3) {
    
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <file_path> <string>", argv[0]);
        fprintf(stderr, "Usage: %s <file_path> <string>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *file_path = argv[1];
    const char *string_to_write = argv[2];

    // Open file
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open file: %s", file_path);
        perror("Failed to open file");
        closelog();
        return 1;
    }

    // Write string to file
    if (fprintf(file, "%s", string_to_write) < 0) {
        syslog(LOG_ERR, "Failed to write to file: %s", file_path);
        perror("Failed to write to file");
        fclose(file);
        closelog();
        return 1;
    }

    // Log success message
    syslog(LOG_DEBUG, "Writing '%s' to '%s'", string_to_write, file_path);

    // Close file and syslog
    fclose(file);
    closelog();

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 8192 // Adjust buffer size as needed

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <sock_file ><file_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *socket_file_path = argv[1];
    const char *file_name = argv[2];

    int sockfd;
    struct sockaddr_un server_addr;
    FILE *file;
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read;

    if (access(socket_file_path, F_OK) == -1) {
        // The socket file does not exist
        printf("Socket file does not exist at: %s\n", socket_file_path);
        exit(EXIT_FAILURE);
    }

    // Create a UNIX domain socket
    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_file_path, sizeof(server_addr.sun_path) - 1);

    // Open the file for reading
    file = fopen(file_name, "rb");
    if (file == NULL) {
        perror("File open failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        ssize_t bytes_sent = sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr,
            sizeof(server_addr));
        if (bytes_sent == -1) {
            perror("Sendto failed");
            fclose(file);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    // Close the file and the socket when done
    fclose(file);
    close(sockfd);

    return 0;
}

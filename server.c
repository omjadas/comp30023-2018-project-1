#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

int readFile(char **, char *);

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    struct sockaddr_in serv_addr, cli_addr;
    char *root_dir;
    char request_buffer[BUFFER_SIZE];
    socklen_t clilen;

    if (argc < 3) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    /* Create TCP socket */

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    memset((char *)&serv_addr, '0', sizeof(serv_addr));

    portno = atoi(argv[1]);

    root_dir = argv[2];

    /* Create address we're going to listen on (given port number)
     - converted to network byte order & any IP address for
     this machine */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); // store in machine-neutral format

    /* Bind address to the socket */

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /* Listen on socket - means we're ready to accept connections -
     incoming connection requests will be queued */

    listen(sockfd, 10);

    clilen = sizeof(cli_addr);

    /* Accept a connection - block until a connection is ready to
     be accepted. Get back a new file descriptor to communicate on. */

    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

    if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    memset(request_buffer, '0', BUFFER_SIZE);

    read(newsockfd, request_buffer, BUFFER_SIZE);

    char file_path[BUFFER_SIZE];
    int i;
    for (i = 4; request_buffer[i] != ' '; i++) {
        file_path[i - 4] = request_buffer[i];
    }

    printf("%s%s\n", root_dir, file_path);

    /* close socket */

    close(sockfd);
    return 0;
}

int readFile(char **output, char *path) {
    FILE *file_ptr;
    long file_len;

    file_ptr = fopen(path, "rb");
    fseek(file_ptr, 0, SEEK_END);
    file_len = ftell(file_ptr);
    rewind(file_ptr);

    *output = (char *)malloc((file_len + 1) * sizeof(char));
    fread(*output, file_len, 1, file_ptr);
    fclose(file_ptr);
    return file_len;
}
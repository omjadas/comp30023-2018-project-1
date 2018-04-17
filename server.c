#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

int read_file(char **, char *);
char *get_dir(char *);
const char *mime_type(char *);
void send_file(char *, int, int);

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
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno); // store in machine-neutral format

    /* Bind address to the socket */

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    clilen = sizeof(cli_addr);

    while (1) {
        /* Listen on socket - means we're ready to accept connections -
         incoming connection requests will be queued */

        listen(sockfd, 1);

        /* Accept a connection - block until a connection is ready to
         be accepted. Get back a new file descriptor to communicate on. */

        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        memset(request_buffer, '0', BUFFER_SIZE);

        read(newsockfd, request_buffer, BUFFER_SIZE);

        // printf("%s\n", request_buffer);

        char *file_path = get_dir(request_buffer);

        char full_path[BUFFER_SIZE * 2];

        strcpy(full_path, root_dir);
        strcat(full_path, file_path);

        char *file;
        int file_len;

        file_len = read_file(&file, full_path);

        if (file_len >= 0) {
            printf("200 OK\n");
            char header[BUFFER_SIZE * 4];
            strcat(header, "HTTP/1.0 200 OK\r\n");
            strcat(header, mime_type(full_path));
            write(newsockfd, header, strlen(header));
            write(newsockfd, file, file_len);
            printf("%s", header);
            header[0] = '\0';
            free(file);
        } else {
            printf("404 Not Found\n");
            write(newsockfd, "HTTP/1.0 404 Not Found\r\n", 25);
        }

        close(newsockfd);
        free(file_path);
    }

    /* close socket */

    close(sockfd);
    return 0;
}

int read_file(char **output, char *path) {
    FILE *file_ptr;
    long file_len;

    // printf("%s\n", path);

    if (file_ptr = fopen(path, "rb")) {
        fseek(file_ptr, 0, SEEK_END);
        file_len = ftell(file_ptr);
        rewind(file_ptr);

        *output = (char *)malloc((file_len + 1) * sizeof(char));
        fread(*output, file_len, 1, file_ptr);
        fclose(file_ptr);
        return file_len;
    }
    return -1;
}

char *get_dir(char *request) {

    // printf("\n%s\n", request);

    char *path = malloc(BUFFER_SIZE * sizeof(char));
    int i;
    for (i = 4; request[i] != ' '; i++) {
        path[i - 4] = request[i];
    }
    path[i - 4] = '\0';
    return path;
}

const char *mime_type(char *path) {
    char *dot = strrchr(path, '.');
    dot++;

    printf("%s", dot);
    if (strcmp(dot, "html") == 0) {
        return "Content-Type: text/html\r\n\r\n";
    } else if (strcmp(dot, "jpg") == 0) {
        return "Content-Type: image/jpeg\r\n\r\n";
    } else if (strcmp(dot, "css") == 0) {
        return "Content-Type: text/css\r\n\r\n";
    } else if (strcmp(dot, "js") == 0) {
        return "Content-Type: application/javascript\r\n\r\n";
    }
    return 0;
}

void send_file(char *file, int file_len, int sockfd) {
}
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 2048 /** The size of the file buffer */

/** Function prototypes */
int read_file(char **, char *);
char *get_dir(char *);
const char *mime_type(char *);
void *worker(void *);
void send_file(char *, int);

/** Struct that contains relevant arguments for the worker function */
struct arg_struct {
    int newsockfd;
    char *root_dir;
};

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    struct sockaddr_in serv_addr, cli_addr;
    char *root_dir;
    socklen_t clilen;
    
    /** Test whether all command line arguments were provided */
    if (argc < 3) {
        fprintf(stderr, "ERROR, missing arguments\n");
        exit(1);
    }

    /** Create TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    memset((char *)&serv_addr, '0', sizeof(serv_addr));

    portno = atoi(argv[1]);

    root_dir = argv[2];

    /**
     * Create address we're going to listen on (given port number)
     * converted to network byte order & any IP address for
     * this machine
     */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno); // store in machine-neutral format

    /** Bind address to the socket */
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    clilen = sizeof(cli_addr);

    /**  */
    while (1) {
        pthread_t tid;

        /*
         * Listen on socket - means we're ready to accept connections -
         * incoming connection requests will be queued
         */
        listen(sockfd, 1);

        /*
         * Accept a connection - block until a connection is ready to
         * be accepted. Get back a new file descriptor to communicate on.
         */
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        /** Create a worker thread */
        struct arg_struct args;
        args.newsockfd = newsockfd;
        args.root_dir = root_dir;
        pthread_create(&tid, NULL, worker, (void *)&args);
    }

    /** close socket */
    close(sockfd);
    return 0;
}

/**
 * Function: worker
 * ----------------
 * 
 * 
 * @param arguments Struct containing relevant arguments
 */
void *worker(void *arguments) {
    int n;
    char request_buffer[BUFFER_SIZE];
    struct arg_struct *args = arguments;

    int newsockfd = args->newsockfd;
    char *root_dir = args->root_dir;

    memset(request_buffer, '0', sizeof(request_buffer));

    n = read(newsockfd, request_buffer, sizeof(request_buffer) - 1);

    if (n < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }

    char *file_path = get_dir(request_buffer);

    char full_path[BUFFER_SIZE * 2];

    strcpy(full_path, root_dir);
    strcat(full_path, file_path);

    free(file_path);

    if (access(full_path, F_OK) != -1) {
        printf("200 OK\n");
        char header[BUFFER_SIZE * 4];
        header[0] = '\0';
        strcat(header, "HTTP/1.0 200 OK\r\n");
        strcat(header, mime_type(full_path));
        n = write(newsockfd, header, strlen(header));
        if (n < 0) {
            perror("ERROR writing to socket");
            exit(1);
        }
        send_file(full_path, newsockfd);
    } else {
        printf("404 Not Found\n");
        n = write(newsockfd, "HTTP/1.0 404 Not Found\r\n", 25);
        if (n < 0) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }

    close(newsockfd);

    return NULL;
}

/**
 * Function: send_file
 * -------------------
 * Sends the file located at path to the socket newsockfd.
 * 
 * @param path      A string containing the path of the file to be sent
 * @param newsockfd The socket that the file is being sent over
 */
void send_file(char *path, int newsockfd) {
    char buffer[BUFFER_SIZE];
    int file = open(path, O_RDONLY);
    while (1) {
        int bytes_read = read(file, buffer, sizeof(buffer));

        if (bytes_read == 0) {
            break;
        }

        void *p = buffer;
        while (bytes_read > 0) {
            int written = write(newsockfd, p, bytes_read);
            if (written < 0) {
                perror("ERROR writing to socket");
                exit(1);
            }
            bytes_read -= written;
            p += written;
        }
    }
}

/**
 * Function: get_dir
 * -----------------
 * Parses the relative path of the requested file from the HTTP request.
 * 
 * @param request   HTTP request
 * @return          The relative path of the requested file
 */
char *get_dir(char *request) {
    char *path = malloc(BUFFER_SIZE * sizeof(char));
    int i;
    for (i = 4; request[i] != ' '; i++) {
        path[i - 4] = request[i];
    }
    path[i - 4] = '\0';
    return path;
}

/**
 * Function: mime_type
 * -------------------
 * Returns the relevant Content-Type header for .html,.jpg, .css, and .js files.
 * 
 * @param path  The path of the file being requested
 * @return      Content-Type header or newline if not recognised
 */
const char *mime_type(char *path) {
    char *dot = strrchr(path, '.');
    dot++;

    if (strcmp(dot, "html") == 0) {
        return "Content-Type: text/html\r\n\r\n";
    } else if (strcmp(dot, "jpg") == 0) {
        return "Content-Type: image/jpeg\r\n\r\n";
    } else if (strcmp(dot, "css") == 0) {
        return "Content-Type: text/css\r\n\r\n";
    } else if (strcmp(dot, "js") == 0) {
        return "Content-Type: application/javascript\r\n\r\n";
    }
    return "\r\n";
}
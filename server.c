#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 2048 /** The size of the file buffer */
#define NUM_PARAMS 3     /** Expected number of command line arguments */

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
    if (argc < NUM_PARAMS) {
        fprintf(stderr, "ERROR, missing arguments\n");
        exit(1);
    }

    /** Read values from command line arguments */
    portno = atoi(argv[1]);
    root_dir = argv[2];

    /**
     * Create address we're going to listen on (given port number)
     * converted to network byte order & any IP address for
     * this machine
     */
    memset((char *)&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno); /** store in machine-neutral format */

    /** Create TCP socket and check for errors while opening */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    /** Bind address to the socket */
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    clilen = sizeof(cli_addr);

    /** Loop */
    while (1) {
        pthread_t tid;

        /**
         * Listen on socket - means we're ready to accept connections -
         * incoming connection requests will be queued
         */
        listen(sockfd, 1);

        /**
         * Accept a connection - block until a connection is ready to
         * be accepted. Get back a new file descriptor to communicate on.
         */
        if ((newsockfd =
                 accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        /** Create a worker thread */
        struct arg_struct args;
        args.newsockfd = newsockfd;
        args.root_dir = root_dir;
        pthread_create(&tid, NULL, worker, (void *)&args);
    }

    /** Close socket */
    close(sockfd);
    return 0;
}

/**
 * Function: worker
 * ----------------
 * Reads and appropriately responds to a HTTP request.
 *
 * @param arguments Struct containing relevant arguments
 */
void *worker(void *arguments) {
    int n;
    char request_buffer[BUFFER_SIZE];
    struct arg_struct *args = arguments;

    /** Read values from arguments */
    int newsockfd = args->newsockfd;
    char *root_dir = args->root_dir;

    memset(request_buffer, '0', sizeof(request_buffer));

    /** Reads request from socket */
    if ((n = read(newsockfd, request_buffer, sizeof(request_buffer) - 1)) < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }

    /** Gets the full path of the requested file */
    char *file_path = get_dir(request_buffer);
    char full_path[BUFFER_SIZE * 2];
    strcpy(full_path, root_dir);
    strcat(full_path, file_path);
    free(file_path);

    /** 
     * If file exists "200 OK", MIME type, and file are sent, otherwise
     * "404 Not Found" is sent.
     */
    if (access(full_path, F_OK) != -1) {
        printf("200 OK\n");
        char header[BUFFER_SIZE];
        header[0] = '\0';
        strcat(header, "HTTP/1.0 200 OK\r\n");
        strcat(header, mime_type(full_path));

        /** Writes header to socket */
        if ((n = write(newsockfd, header, strlen(header))) < 0) {
            perror("ERROR writing to socket");
            exit(1);
        }
        send_file(full_path, newsockfd);
    } else {
        printf("404 Not Found\n");

        /** Write 404 to socket */
        if ((n = write(newsockfd, "HTTP/1.0 404 Not Found\r\n", 25)) < 0) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }

    /** Close connection */
    close(newsockfd);

    return NULL;
}

/**
 * Function: send_file
 * -------------------
 * Sends the file located at path to the socket newsockfd.
 *
 * @param path      A string containing the path of the file to be sent
 * @param newsockfd The socket that the file is being sent to
 */
void send_file(char *path, int newsockfd) {
    char buffer[BUFFER_SIZE];
    int file = open(path, O_RDONLY);
    while (1) {
        int bytes_read = read(file, buffer, sizeof(buffer));

        /** Check if file has finished sending */
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
/*
 * simpleserver.c - a minimal HTTP server that serves static and
 *                  dynamic content with the GET method. Neither
 *                  robust, secure, nor modular. Use for instructional
 *                  purposes only.
 *
 *                  compile : gcc -pthread -o server simpleserver.c
 *
 *                  usage: ./server
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>

#define SERVERPORT 10001 /* Change the port to whatever you like */
#define BUFSIZE 4096
#define SERVER_BACKLOG 100 /* Server request queue */


/* Function Prototypes */
void * handle_connection(void* p_client_socket);
void error(char *msg);
void cerror(FILE *stream, char *cause, char *errno, char *shortmsg, char *longmsg);


/* main function */
int main() {
    /* variables for connection management */
    int server_socket; /* server socket */
    int client_socket; /* client socket */
    int address_size; /* address size of client address */
    struct sockaddr_in server_address; /* Server address */
    struct sockaddr_in client_address; /* Client address */


    /* open socket descriptor */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0){
        error("ERROR opening socket");
    }

    /* bind port to socket */
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVERPORT);
    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
        error("ERROR on binding");
    }

    /* get us ready to accept connection requests */
    if (listen(server_socket, SERVER_BACKLOG) < 0){ /* allow 100 requests to queue up */
        error("ERROR on listen");
    }


    /*
     * main loop: wait for a connection request, parse HTTP,
     * serve requested content, close connection.
     */
    while (true){

        address_size = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *) &client_address, (socklen_t*)&address_size);
        if (client_socket < 0){
            error("ERROR on accept");
        }
        else{
            /* Create a new thread per each connection */
            pthread_t new_thread;
            int *p_client = malloc(sizeof(int));
            *p_client = client_socket;

            pthread_create(&new_thread, NULL, handle_connection, p_client);

        }
    }
}


/*
 * error - wrapper for perror used for bad syscalls
 */
void error(char *msg){
    perror(msg);
    exit(1);
}


/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: a void pointer to the socket connected to the client */
/**********************************************************************/
void * handle_connection(void* p_client_socket){
    /* variables for connection I/O */
    FILE *stream;          /* stream version of childfd */
    char buf[BUFSIZE];     /* message buffer */
    char method[BUFSIZE];  /* request method */
    char uri[BUFSIZE];     /* request uri */
    char version[BUFSIZE]; /* request method */
    char filename[BUFSIZE];/* path derived from uri */
    char filetype[BUFSIZE];/* path derived from uri */
    char cgiargs[BUFSIZE]; /* cgi argument list */
    char *p;               /* temporary pointer */
    struct stat sbuf;      /* file status */
    int fd;                /* static content filedes */
    int client_socket = *((int*)p_client_socket); /* change from void pointer to int*/

    free(p_client_socket); /* free the memory allocated to client socket ponter */


    /* open the child socket descriptor as a stream */
    if ((stream = fdopen(client_socket, "r+")) == NULL){
        error("ERROR on fdopen");
    }

    /* get the HTTP request line */
    fgets(buf, BUFSIZE, stream);
    printf("%s", buf);
    sscanf(buf, "%s %s %s\n", method, uri, version);

    /* The server only supports the GET method */
    if (strcasecmp(method, "GET") != 0) {
        cerror(stream, method, "405", "Method Not Allowed",
                "Server does not implement this method.");
        fclose(stream);
        close(client_socket);
        return NULL;
    }

    /* read (and ignore) the HTTP headers */
    fgets(buf, BUFSIZE, stream);
    printf("%s", buf);
    while(strcmp(buf, "\r\n") != 0) {
        fgets(buf, BUFSIZE, stream);
        printf("%s", buf);
    }

    /* parse the uri [crufty] */
    if (!strstr(uri, "cgi-bin")) { /* static content */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/'){
            strcat(filename, "index.html");
        }
    }
    else { /* dynamic content */
        cerror(stream, filename, "418", "I'm a teapot",
               "Server does not support dynamic content");
    }

    /* make sure the file exists */
    if (stat(filename, &sbuf) < 0) {
        cerror(stream, filename, "404", "Not found",
               "Server couldn't find this file");
        fclose(stream);
        close(client_socket);
        return NULL;
    }

    /* serve static content */
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpg");
    else
        strcpy(filetype, "text/plain");

    /* print response header */
    fprintf(stream, "HTTP/1.1 200 OK\n");
    fprintf(stream, "Server: Simple Web Server\n");
    fprintf(stream, "Content-length: %d\n", (int)sbuf.st_size);
    fprintf(stream, "Content-type: %s\n", filetype);
    fprintf(stream, "\r\n");
    fflush(stream);

    /* Use mmap to return arbitrary-sized response body */
    fd = open(filename, O_RDONLY);
    p = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    fwrite(p, 1, sbuf.st_size, stream);
    munmap(p, sbuf.st_size);


    fclose(stream);
    close(client_socket);
    return NULL;
}

/*
 * cerror - returns an error message to the client
 */
void cerror(FILE *stream, char *cause, char *errno, char *shortmsg, char *longmsg) {
    fprintf(stream, "HTTP/1.1 %s %s\n", errno, shortmsg);
    fprintf(stream, "Content-type: text/html\n");
    fprintf(stream, "\n");
    fprintf(stream, "<html><title>Server Error</title>");
    fprintf(stream, "<body bgcolor=""ffffff"">\n");
    fprintf(stream, "%s: %s\n", errno, shortmsg);
    fprintf(stream, "<p>%s: %s\n", longmsg, cause);
    fprintf(stream, "<hr><em>The Simple Web server</em>\n");
}

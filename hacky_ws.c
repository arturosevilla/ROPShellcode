#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_BUFFER 500
#define HTTP_PORT 8080
#define BACKLOG 20

int create_server(void)
{
    struct sockaddr_in server_address;
    int server, reuse = 1;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(HTTP_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY; // any address

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        return server;
    }
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(server, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        return -1;
    }

    if (listen(server, BACKLOG) < 0) {
        return -1;
    }

    return server;
}

int accept_client(int server)
{
    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0 && errno != EINTR) {
            return -1;
        } else if (client < 0 && errno == EINTR) {
            continue;
        }
        return client;
    }
}

void get_line(int client, char *buffer)
{
    char c = 0;
    printf("getline\n");
    while (c != '\n') {
        if (recv(client, &c, 1, 0) < 0) {
            return;
        }
        *buffer++ = c;
    }
}

void handle_get(char * buffer, int client)
{
    unsigned long length = 0;
    char response_buffer[MAX_BUFFER];
    char *query = buffer + 3;
    while (isspace(*query)) {
        query++;
    }
    if (*query == '/' && isspace(*(query + 1))) {
        sprintf(response_buffer, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n");
        printf("%s\n", response_buffer);
        write(client, response_buffer, strlen(response_buffer));
        strcpy(buffer,
               "<html><body><form action=\"/\" method=\"post\">"
               "<textarea id=\"data\" name=\"data\"></textarea>"
               "<input type=\"submit\" value=\"Go\" /></form>"
               "</body></html>");
        printf("%s\n", buffer);
        length = strlen(buffer);
        sprintf(response_buffer, "Content-Length: %lu\r\n\r\n", length);
        write(client, response_buffer, strlen(response_buffer));
        write(client, buffer, length);
    } else {
        printf("404\n");
        sprintf(buffer, "HTTP/1.0 404 Not Found\r\n\r\n");
        write(client, buffer, strlen(buffer));
    }
}

void handle_post(char * buffer, int client)
{
    char response_buffer[MAX_BUFFER];
    unsigned long length = 0;
    char *location = 0;
    printf("post");
    while (!location) {
        get_line(client, buffer);
        printf("%s\n", buffer);
        if (strncasecmp("Content-Length:", buffer, 15) == 0) {
            length = strtoul(buffer + 15, NULL, 0);
            printf("Length: %lu\n", length);
        }
        if (strncmp("\r\n", buffer, 2) == 0) {
            if (length == 0) {
               break;
            }
            read(client, buffer, length);
            location = strstr(buffer, "data=");
        }
    }
    if (location == 0) {
        sprintf(buffer, "HTTP/1.0 400 Bad Request\r\n\r\n");
        write(client, buffer, strlen(buffer));
        return;
    }
    printf("got data");
    *(location + length) = 0; // make C string
    sprintf(response_buffer, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
    write(client, response_buffer, strlen(response_buffer));
    sprintf(response_buffer, "<html><body><p>%s</p></body></html>", location + 5);
    write(client, response_buffer, strlen(response_buffer));
}

void handle_client(int client)
{
    char buffer[MAX_BUFFER];
    memset(buffer, 0, MAX_BUFFER);
    get_line(client, buffer);
    printf("%p\n", buffer);
    if (strncmp(buffer, "GET", 3) == 0) {
        handle_get(buffer, client);
    } else if (strncmp(buffer, "POST", 4) == 0) {
        handle_post(buffer, client);
    } else {
        sprintf(buffer, "HTTP/1.0 501 Not Implemented\r\n\r\n");
        write(client, buffer, strlen(buffer));
    }
}

int main(int argc, char **argv)
{
    int server = create_server();
    if (server < 0) {
        perror("create_server");
        return EXIT_FAILURE;
    }
    while (1) {
        int child;
        int client = accept_client(server);
        printf("client\n");
#ifndef DEBUG
        if ((child = fork()) < 0) {
            perror("fork");
            return EXIT_FAILURE;
        } else if (child == 0) {
#endif
            printf("in child\n");
            handle_client(client);
            close(client);
#ifndef DEBUG
            return 0;
        } else {
            close(client);
        }
#endif
    }
}


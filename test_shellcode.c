#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


void *xmalloc(size_t size)
{
    char *buff= malloc(size);
    if (!buff) {
        perror("malloc");
        abort();
    }
    return buff;
}

int main(int argc, char **argv)
{
    struct stat st_data;
    char *buff;
    int shellcode;
    int dup1, dup2;
    unsigned int size;
    if (argc != 2) {
        fprintf(stderr, "usage: %s shellcode\n", argv[0]);
        return EXIT_FAILURE;
    }
    dup1 = open("/dev/tty", O_RDWR);
    if (dup1 < 0) {
        perror("open console 1");
        return EXIT_FAILURE;
    }
    dup2 = open("/dev/tty", O_RDWR);
    if (dup2 < 0) {
        perror("open console 2");
        return EXIT_FAILURE;
    }
    if (stat(argv[1], &st_data) < 0) {
        perror("stat");
        return EXIT_FAILURE;
    }
    size = st_data.st_size;
    buff = xmalloc(size);
    shellcode = open(argv[1], O_RDONLY);
    if (shellcode < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    if (read(shellcode, buff, size) != size) {
        perror("read");
        return EXIT_FAILURE;
    }

    ((void (*)(void))buff)();
    return 0;
}

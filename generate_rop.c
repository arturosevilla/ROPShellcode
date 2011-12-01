#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define RET_OPCODE 0xC3
#define RETIM_OPCODE 0xC2
#define RETFAR_OPCODE 0xCB
#define RETFARIM_OPCODE 0xCA

#define set_sp_relevant(address) \
    __asm__ volatile ("movq %0, %%rbp" : : "g"(address))

typedef struct _ret_node {
    struct _ret_node *next;
    unsigned long long address;
    unsigned int type;
} ret_node;

static ret_node *start_node;

static inline void *xmalloc(size_t size)
{
    void *data = malloc(size);
    if (!data) {
        perror("malloc");
        abort();
    }
    return data;
}

void insert_ret(unsigned long long address, unsigned int type)
{
    ret_node *n_node = xmalloc(sizeof(*start_node));
    n_node->address = address;
    n_node->next = start_node;
    n_node->type = type;
    start_node = n_node;
}

void clean_ret_list(void)
{
    if (!start_node) {
        return;
    }
    
    while (start_node->next) {
        ret_node *tmp = start_node;
        start_node = start_node->next;
        free(tmp);
    }

    free(start_node);
}

void get_bytes(char *assem, unsigned char *code, unsigned long *size)
{
    const char *tmpfile = ".compiler_rop.s";
    const char *tmpobj = ".compiler_rop.o";
    const char *tmpbin = ".compiler_bin.bin";
    struct stat st_data;
    int to_compile = open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC);
    int from_compile;
    if (to_compile < 0) {
        perror("open");
        return;
    }

    if (write(to_compile, assem, strlen(assem)) < 0) {
        return;
    }

    write(to_compile, "\n", 1);
    close(to_compile);

    system("gcc -c .compiler_rop.s && ld -o .compiler_bin.bin .compiler_rop.o"
           " --oformat=binary -Ttext=0 2>/dev/null && rm -f .compiler_rop.s "
           ".compiler_rop.o");

    from_compile = open(tmpbin, O_RDONLY);
    if (from_compile < 0) {
        return;
    }

    if (stat(tmpbin, &st_data) < 0) {
        return;
    }

    if (*size < st_data.st_size) {
        fprintf(stderr, "code too big\n");
        abort();
    }

    *size = st_data.st_size;
    if (read(from_compile, code, *size) != *size) {
        perror("read");
        abort();
    }

    close(from_compile);
    /*
    if (unlink(tmpfile) < 0) {
        perror("unlink s");
    }
    if (unlink(tmpobj) < 0) {
        perror("unlink o");
    }
    if (unlink(tmpbin) < 0) {
        perror("unlink bin");
    }
    */
}

void print_code(unsigned char *code, unsigned long size)
{
    unsigned long i = 0;
    for (i = 0; i < size; i++) {
        printf("%x", *code++);
    }
    printf("\n");
}

unsigned char *look_in_libc(unsigned char *code, unsigned long size)
{
    ret_node *look = start_node;
    
    while (look) {
        if (look->type == RET_OPCODE) {
            unsigned char *data = (unsigned char *)look->address - size;
            if (memcmp(data, code, size) == 0) {
                return data;
            }
        }

        look = look->next;
    }
    fprintf(stderr, "not found\n");
    return 0;
}

void find_all_ret(unsigned char *start, unsigned long num_pages)
{
    unsigned char *end = start + num_pages * PAGE_SIZE;
    unsigned long count_ret = 0, count_im = 0, count_far = 0, count_far_im = 0;
    printf("Looking ret codes between %p - %p\n", start, end);
    while (start < end) {
        switch (*start) {
            case RET_OPCODE:
                insert_ret((unsigned long long)start, RET_OPCODE);
                count_ret++;
                break;
            case RETIM_OPCODE:
                insert_ret((unsigned long long)start, RETIM_OPCODE);
                count_im++;
                break;
            case RETFAR_OPCODE:
                insert_ret((unsigned long long)start, RETFAR_OPCODE);
                count_far++;
                break;
            case RETFARIM_OPCODE:
                insert_ret((unsigned long long)start, RETFARIM_OPCODE);
                count_far_im++;
                break;
        }
        start++;
    }
    printf("Found %lu total ret codes.\n", count_ret + count_im + count_far +
           count_far_im);
    printf("Found %lu ret codes.\n", count_ret);
    printf("Found %lu retimm codes.\n", count_im);
    printf("Found %lu retfar codes.\n", count_far);
    printf("Found %lu retimm far codes.\n", count_far_im);
}

int main(int argc, char **argv)
{
    unsigned char *start_libc;
    unsigned long num_pages;
    unsigned char code[100];
    unsigned char generated[500];
    unsigned long size = 100;
    unsigned char *address, *gen = generated, *pop_rax, *sysenter;
    const char *hello = "Hello, world!\n";
    if (argc != 3) {
        fprintf(stderr, "usage: %s start_libc num_pages\n", argv[0]);
        return EXIT_FAILURE;
    }

    start_libc = (unsigned char *)strtoull(argv[1], NULL, 0);
    num_pages = strtoul(argv[2], NULL, 0);

    find_all_ret(start_libc, num_pages);

    get_bytes("retq", code, &size);
    print_code(code, size);
    size = 100;

    get_bytes("syscall", code, &size);
    sysenter = look_in_libc(code, size);
    if (!sysenter) {
        printf(":(\n");
        return EXIT_FAILURE;
    }
    size = 100;

    //hello world in ROP!
    get_bytes("popq %rax", code, &size);
    pop_rax = address = look_in_libc(code, size);
    memcpy(gen, &address, sizeof(address));
    gen += sizeof(address);
    *((unsigned long long *)gen) = 1ULL; // syscall code
    gen += sizeof(unsigned long long);
    size = 100;
    get_bytes("popq %rdi", code, &size);
    address = look_in_libc(code, size);
    memcpy(gen, &address, sizeof(address));
    gen += sizeof(address);
    *((unsigned long long *)gen) = 1ULL; // stdout
    gen += sizeof(address);
    size = 100;
    get_bytes("popq %rsi", code, &size);
    address = look_in_libc(code, size);
    memcpy(gen, &address, sizeof(address));
    gen += sizeof(address);
    *((unsigned long long *)gen) = (unsigned long long)(generated + 500 - 
            strlen(hello));
    gen += sizeof(address);
    size = 100;
    get_bytes("popq %rdx", code, &size);
    address = look_in_libc(code, size);
    memcpy(gen, &address, sizeof(address));
    gen += sizeof(address);
    *((unsigned long long *)gen) = 14ULL; // Hello, world!\n
    gen += sizeof(address);
    memcpy(gen, &sysenter, sizeof(sysenter));
    gen += sizeof(address);
    memcpy(gen, &pop_rax, sizeof(pop_rax));
    gen += sizeof(address);
    *((unsigned long long *)gen) = 60ULL; // syscall exit
    gen += sizeof(address);
    memcpy(gen, &sysenter, sizeof(sysenter));
    size = 100;
    memcpy(generated + 500 - strlen(hello), hello, strlen(hello));

    clean_ret_list();
    set_sp_relevant(generated - sizeof(void *));
    return 0;
}

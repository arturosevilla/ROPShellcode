#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *esp(void)
{
    asm("mov %rsp, %rax");
}

void * locate_var(char **env, char *var)
{
    while (*env != NULL) {
        if (strncmp(*env, var, strlen(var)) == 0) {
            return *env;
        }
        env++;
    }
    return NULL;
}

int main(int argc, char **argv, char **env)
{
    printf("RSP: %p\n", esp());
    if (argc == 2) {
        void *env_var = locate_var(env, argv[1]);
        printf("env[%s]: %p\n", argv[1], env_var);
        printf("Difference between printf and env[%s]: %tx\n", argv[1],
               env_var - (void *)printf);
    }
    printf("printf: %p\n", (void *)printf);
    return 0;
}

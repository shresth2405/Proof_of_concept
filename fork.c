#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main()
{
    int *ptr = malloc(4096);

    *ptr = 42;

    printf("Parent PID: %d\n", getpid());
    printf("Shared address: %p\n", ptr);

    pid_t pid = fork();

    if (pid == 0)
    {
        printf("Child PID: %d\n", getpid());

        sleep(100);

        printf("Child writing to memory...\n");
        *ptr = 100;

        sleep(100);
    }
    else
    {
        sleep(200);
    }

    return 0;
}


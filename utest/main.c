#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int pid = fork();
    if (pid == 0)
        sleep(2);

    printf("argc: %i\n", argc);
    for (int i = 0; i < argc; i++)
        printf("%i: %s\n", i, argv[i]);

    FILE* tty = fopen("/dev/tty0", "r+");
    fwrite("menel\n", 6, 1, tty);

    sleep(2);

    return 0;
}
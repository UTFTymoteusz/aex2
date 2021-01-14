#include <sys/mman.h>
#include <sys/types.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void test_kill();
void test_mem();

int main(int argc, char* argv[]) {
    char buffer[2048];
    getcwd(buffer, sizeof(buffer));
    printf("cwd: %s\n", buffer);

    chdir("/bin/");

    getcwd(buffer, sizeof(buffer));
    printf("cwd: %s\n", buffer);

    chdir("/dev");

    getcwd(buffer, sizeof(buffer));
    printf("cwd: %s\n", buffer);

    printf("argc: %i\n", argc);
    for (int i = 0; i < argc; i++)
        printf("%i: %s\n", i, argv[i]);

    FILE* tty = fopen("tty0", "r+");
    fwrite("menel\n", 6, 1, tty);

    test_kill();
    // test_mem();
    sleep(1);

    return 0;
}

void signal_entry(int id) {
    printf("signal entered\n");
}

void signal_restore() {
    printf("signal restored\n");
    sigret();
}

void test_kill() {
    pid_t pid = fork();
    if (pid == 0) {
        struct sigaction action = {
            .sa_handler  = signal_entry,
            .sa_restorer = signal_restore,
        };

        sigaction(SIGUSR1, &action, NULL);
        sleep(5);

        exit(66);
    }

    sleep(1);
    kill(pid, SIGUSR1);
}

void test_mem() {
    sleep(1);

    uint16_t* aa = malloc(25);
    printf("aa: 0x%p\n", aa);

    uint16_t* bb = malloc(25);
    printf("bb: 0x%p\n", bb);

    uint16_t* cc = malloc(25);
    printf("cc: 0x%p\n", cc);

    debug_print_pools();
    free(bb);
    debug_print_pools();

    uint16_t* dd = malloc(25);
    printf("dd: 0x%p\n", dd);

    debug_print_pools();
}
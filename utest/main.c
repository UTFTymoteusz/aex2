#include <sys/mman.h>
#include <sys/types.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void test_kill();
void test_mem();

void test_segv(int id, struct siginfo_t* info, void* aaa) {
    printf("segmentation fault (%i) @ 0x%p %i\n", id, info->si_addr, info->si_code);
    sleep(2);
}

void test_ill(int id, struct siginfo_t* info, void* aaa) {
    printf("illegal instruction (%i) @ 0x%p %i\n", id, info->si_addr, info->si_code);
    sleep(2);
}

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

    // test_kill();
    // test_mem();
    sleep(1);

    struct sigaction actionA = {
        .sa_sigaction = test_segv,
        .sa_flags     = SA_SIGINFO,
    };

    sigaction(SIGSEGV, &actionA, NULL);

    struct sigaction actionB = {
        .sa_handler = SIG_DFL,
        //.sa_sigaction = test_ill,
        .sa_flags = 0,
    };

    sigaction(SIGILL, &actionB, NULL);

    asm volatile("ud2");

    int* dasda = (int*) 0x2323232323;
    printf("%i\n", dasda[0]);

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
            .sa_flags    = SA_RESTORER,
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
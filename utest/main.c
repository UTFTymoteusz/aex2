#include <arpa/inet.h>
#include <sys/wait.h>

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASSERT(condition)                                                                       \
    ({                                                                                          \
        if (!(condition)) {                                                                     \
            fprintf(stderr, "%s:%i: Assertion failed! (%s)\n", __FILE__, __LINE__, #condition); \
            exit(EXIT_FAILURE);                                                                 \
        }                                                                                       \
        else {                                                                                  \
            printf("%s:%i: %s\n", __FILE__, __LINE__, #condition);                              \
        }                                                                                       \
    })

void test_file();
void test_inet();
void test_ctype();

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "cloexec") == 0) {
            int a = atoi(argv[2]);
            int b = atoi(argv[3]);

            ASSERT(close(a) == -1);
            ASSERT(close(b) != -1);

            return 0;
        }
    }

    sleep(1);
    test_file();
    test_inet();
    test_ctype();
}

void test_file() {
    DIR* root = opendir("/");
    ASSERT(root);
    ASSERT(closedir(root) == 0);

    int root_fd = open("/", O_RDONLY);
    ASSERT(root_fd != -1);
    ASSERT(close(root_fd) == 0);

    ASSERT(open("/", O_WRONLY) == -1);
    ASSERT(open("/", O_RDWR) == -1);

    char buff[8];
    int  des[2];
    ASSERT(pipe(des) == 0);
    ASSERT(read(des[1], buff, 6) == -1);
    ASSERT(write(des[1], "abcdef", 6) == 6);

    ASSERT(read(des[0], buff, 6) == 6);
    ASSERT(write(des[0], "abcdef", 6) == -1);
    ASSERT(memcmp(buff, "abcdef", 6) == 0);

    pid_t ff = fork();
    if (ff == 0) {
        int fdtestA = open("/", O_RDONLY);
        int fdtestB = open("/", O_RDONLY);

        ASSERT(fdtestA != -1);
        ASSERT(fdtestB != -1);

        ASSERT(fcntl(fdtestA, F_SETFD, FD_CLOEXEC) != -1);

        char buffA[8];
        char buffB[8];

        sprintf(buffA, "%i", fdtestA);
        sprintf(buffB, "%i", fdtestB);

        char* const argv[5] = {
            "utest", "cloexec", buffA, buffB, NULL,
        };

        ASSERT(execve("utest", argv, NULL) != -1);
    }
    else {
        int stat;
        wait(&stat);

        ASSERT(stat == 0);
    }
}

void test_inet() {
    ASSERT(htonl(0x00000001) == 0x01000000);
    ASSERT(htons(0x0001) == 0x0100);
    ASSERT(ntohl(0x00000001) == 0x01000000);
    ASSERT(ntohs(0x0001) == 0x0100);
}

void test_ctype() {
    ASSERT(isalnum('a'));
    ASSERT(isalnum('Y'));
    ASSERT(isalnum('0'));
    ASSERT(isalnum('5'));
    ASSERT(isalnum('9'));
    ASSERT(!isalnum('.'));
    ASSERT(!isalnum(':'));
    ASSERT(!isalnum(' '));
    ASSERT(isalpha('a'));
    ASSERT(isalpha('Y'));
    ASSERT(!isalpha('2'));
    ASSERT(isblank(' '));
    ASSERT(isblank('\t'));
    ASSERT(!isblank('\n'));
    ASSERT(!isblank('a'));
    ASSERT(iscntrl('\0'));
    ASSERT(iscntrl('\a'));
    ASSERT(iscntrl('\a'));
    ASSERT(iscntrl('\r'));
    ASSERT(iscntrl('\n'));
    ASSERT(iscntrl('\e'));
    ASSERT(isdigit('0'));
    ASSERT(isdigit('9'));
    ASSERT(isdigit('5'));
    ASSERT(!isdigit('a'));
    ASSERT(!isdigit('b'));
    ASSERT(!isdigit('H'));
    ASSERT(!isdigit('Y'));
    ASSERT(isgraph('.'));
    ASSERT(isgraph('2'));
    ASSERT(isgraph('u'));
    ASSERT(isgraph('p'));
    ASSERT(!isgraph(' '));
    ASSERT(islower('a'));
    ASSERT(islower('o'));
    ASSERT(!islower('G'));
    ASSERT(isprint('G'));
    ASSERT(isprint('h'));
    ASSERT(isprint(' '));
    ASSERT(!isprint('\t'));
    ASSERT(isspace(' '));
    ASSERT(isspace('\t'));
    ASSERT(isspace('\v'));
    ASSERT(!isspace('a'));
    ASSERT(isupper('A'));
    ASSERT(isupper('I'));
    ASSERT(isupper('P'));
    ASSERT(!isupper(' '));
    ASSERT(!isupper('.'));
    ASSERT(!isupper('a'));
    ASSERT(isxdigit('1'));
    ASSERT(isxdigit('7'));
    ASSERT(isxdigit('a'));
    ASSERT(isxdigit('E'));
    ASSERT(isxdigit('f'));
    ASSERT(isxdigit('F'));
    ASSERT(!isxdigit('u'));
    ASSERT(!isxdigit('U'));
    ASSERT(_toupper('a') == 'A');
    ASSERT(_toupper('z') == 'Z');
    ASSERT(_tolower('A') == 'a');
    ASSERT(_tolower('Z') == 'z');
}
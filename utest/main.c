#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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
void test_fb();

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
    test_fb();
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

double rando(double min, double max) {
    return min + ((double) rand() / (double) RAND_MAX) * (max - min);
}

void rect(uint32_t* fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t rgb) {
    if (x >= 1280 || y >= 720)
        return;

    if (x + width >= 1280)
        width -= ((x + width) - 1280);

    if (y + height >= 720)
        height -= ((y + height) - 720);

    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++)
            fb[(y + j) * 1280 + x + i] = rgb;
}

void test_fb() {
    ioctl(1, 0x01, 0x01);
    printf("testA\n");

    printf("tty res: %ix%ix%i (%i)\n", ioctl(1, 0x04), ioctl(1, 0x05), ioctl(1, 0x06),
           ioctl(1, 0x07));

    uint32_t* fb = (uint32_t*) mmap(NULL, ioctl(1, 0x07), PROT_READ | PROT_WRITE, 0, 1, 0);
    if (!fb) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 1280 * 720; i++)
        fb[i] = rando(0x000000, 0xFFFFFF);

    for (int i = 0; i < 1000; i++)
        // rect(fb, rando(0, 1280), rando(0, 720), rando(0, 1280), rando(0, 720),
        //     rando(0x000000, 0xFFFFFF));
        rect(fb, rando(0, 1280), rando(0, 720), rando(0, 1280), rando(0, 720),
             0x2b2b34 * rando(0.0, 1.0));
}
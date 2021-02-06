#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef linux
#define ASSERT(condition)                                                                       \
    ({                                                                                          \
        if (!(condition)) {                                                                     \
            fprintf(stderr, "%s:%i: Assertion failed! (%s)\n", __FILE__, __LINE__, #condition); \
            exit(EXIT_FAILURE);                                                                 \
        }                                                                                       \
    })
#else
#include <syscallids.h>
#define ASSERT(condition)                                                                       \
    ({                                                                                          \
        if (!(condition)) {                                                                     \
            perror("Last error");                                                               \
            fprintf(stderr, "%s:%i: Assertion failed! (%s)\n", __FILE__, __LINE__, #condition); \
            syscall(SYS_PANIC);                                                                 \
        }                                                                                       \
    })
#endif

void test_file();

int main(int argc, char* argv[]) {
    sleep(1);

    test_file();

#ifndef linux
    printf("utest passed, woohoo\n");
#endif
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

    int des[2];
    ASSERT(pipe(des) == 0);
    ASSERT(write(des[1], "abcdef", 6) == 6);

    char buff[8];
    ASSERT(read(des[0], buff, 6) == 6);
    ASSERT(memcmp(buff, "abcdef", 6) == 0);
}
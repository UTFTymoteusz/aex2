#include <stdio.h>
#include <unistd.h>

int main() {
    FILE* tty = fopen("/dev/tty0", "r+");
    fwrite("menel\n", 6, 1, tty);

    sleep(1);
    return 0;
}
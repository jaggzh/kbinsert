#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

/* Coded by Chat GPT4 (without any modifications by me, actually) */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <characters>\n", argv[0]);
        return 1;
    }

    int fd = open("/dev/tty", O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    for (size_t i = 0; i < strlen(argv[1]); i++) {
        if (ioctl(fd, TIOCSTI, &argv[1][i]) == -1) {
            perror("ioctl");
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}

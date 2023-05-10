#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

/* Coded by Chat GPT4 (then modified by me) */

int disable_echo(int fd, struct termios *original_termios);
int restore_echo(int fd, struct termios *original_termios);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Usage: %s <str> [...<more strings>]\n"
		       "(multiple command line arguments are inserted,\n"
		       " single-space-separated)\n",
			argv[0]);
		return 1;
	}

	int fd = open("/dev/tty", O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	struct termios original_termios;

	if (disable_echo(fd, &original_termios)) {
		close(fd);
		return 1;
	}

	for (size_t ai=1; ai < argc; ai++) {
		const char *s=argv[ai];
		if (ai > 1) ioctl(fd, TIOCSTI, " "); // space-sep
		for (size_t i=0; i<strlen(s); i++) {
			if (ioctl(fd, TIOCSTI, s+i) == -1) {
				perror("ioctl");
				restore_echo(fd, &original_termios);
				close(fd);
				return 1;
			}
		}
	}

	if (restore_echo(fd, &original_termios)) {
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}

int disable_echo(int fd, struct termios *original_termios) {
	struct termios new_termios;

	// Save the original terminal settings
	if (tcgetattr(fd, original_termios) == -1) {
		perror("tcgetattr");
		return 1;
	}

	// Disable echo
	new_termios = *original_termios;
	new_termios.c_lflag &= ~ECHO;
	if (tcsetattr(fd, TCSANOW, &new_termios) == -1) {
		perror("tcsetattr");
		return 1;
	}

	return 0;
}

int restore_echo(int fd, struct termios *original_termios) {
	// Restore the original terminal settings
	if (tcsetattr(fd, TCSANOW, original_termios) == -1) {
		perror("tcsetattr");
		return 1;
	}

	return 0;
}


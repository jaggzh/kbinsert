#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <ctype.h>

/* Coded by Chat GPT4 (then modified by me) */
#ifdef GUI_SUPPORT
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
/* #include <X11/keysymdef.h> */
#include <X11/keysym.h>
#endif

int disable_echo(int fd, struct termios *original_termios);
int restore_echo(int fd, struct termios *original_termios);
int insert_text_tty(int argc, char *argv[]);
int insert_text_x11(const char* text);

int main(int argc, char *argv[]) {
	int gui_mode = 0;

	if (argc < 2) {
		printf("Usage: %s [-g] <str> [...<strN>>]\n"
		       "Insert strings in keyboard buffer (as if you typed them).\n"
		       "Multiple command line arguments are treated as one long\n"
		       "space-separated string.inserted,\n"
		       "\n"
		       "-g  Enable X11 insertion (instead of only working in the current term)\n"
		       "",
			argv[0]);
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-g") == 0) {
			gui_mode = 1;
			break;
		}
	}

	if (gui_mode) {
#ifdef GUI_SUPPORT
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-g") != 0) {
				insert_text_x11(argv[i]);
			}
		}
#else
		fprintf(stderr, "GUI support not compiled in\n");
		return 1;
#endif
	} else {
		insert_text_tty(argc, argv);
	}
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

int insert_text_tty(int argc, char *argv[]) {
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

#ifdef GUI_SUPPORT
int insert_text_x11(const char* text) {
	Display *display = XOpenDisplay(NULL);
	if (!display) {
		fprintf(stderr, "Cannot open display\n");
		return 1;
	}

	// Simulate keypresses for each character in the string
	for (const char *p = text; *p != '\0'; p++) {
		int shift_pressed = 0;

		// Check if character requires Shift key
		if (isupper(*p) || strchr("!@#$%^&*()_+{}|:\"<>?", *p)) {
			KeyCode shift_keycode = XKeysymToKeycode(display, XK_Shift_L);
			XTestFakeKeyEvent(display, shift_keycode, True, 0); // Shift key press
			shift_pressed = 1;
		}

		// Convert character to keycode and simulate keypress
		KeyCode keycode = XKeysymToKeycode(display, XStringToKeysym((char[]){*p, 0}));
		XTestFakeKeyEvent(display, keycode, True, 0);  // key press
		XTestFakeKeyEvent(display, keycode, False, 0); // key release

		if (shift_pressed) {
			KeyCode shift_keycode = XKeysymToKeycode(display, XK_Shift_L);
			XTestFakeKeyEvent(display, shift_keycode, False, 0); // Shift key release
		}
	}

	XCloseDisplay(display);
	return 0;
}
#endif

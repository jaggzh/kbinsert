#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>
/* Coded by Chat GPT4 (then modified by me, then ran through claude for esc sequences) */
#ifdef GUI_SUPPORT
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
/* #include <X11/keysymdef.h> */
#include <X11/keysym.h>
#endif

int disable_echo(int fd, struct termios *original_termios);
int restore_echo(int fd, struct termios *original_termios);
int insert_text_tty(int argc, char *argv[], int escape_mode);
int insert_text_x11(const char* text, int escape_mode);
char* process_escape_sequences(const char* input, int* error);

int main(int argc, char *argv[]) {
	int gui_mode = 0;
	int escape_mode = 0;
	
	if (argc < 2) {
		printf("Usage: %s [-g] [-e|--escapes] <str> [...<strN>>]\n"
		       "Insert strings in keyboard buffer (as if you typed them).\n"
		       "Multiple command line arguments are treated as one long\n"
		       "space-separated string.inserted,\n"
		       "\n"
		       "Options:\n"
		       "-g          Enable X11 insertion (instead of only working in the current term)\n"
		       "-e, --escapes  Enable escape sequences:\n"
		       "             \\ooo  - octal value (up to 3 digits)\n"
		       "             \\xhh  - hex value (exactly 2 digits)\n"
		       "             \\^c   - control character (c can be upper or lowercase)\n"
		       "             \\\\    - literal backslash\n"
		       "",
			argv[0]);
		return 1;
	}
	
	// Process arguments
	int start_index = 1;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-g") == 0) {
			gui_mode = 1;
			if (i == start_index) start_index++;
		} else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--escapes") == 0) {
			escape_mode = 1;
			if (i == start_index) start_index++;
		}
	}
	
	// Check if there are any strings to process
	if (start_index >= argc) {
		fprintf(stderr, "No text to insert. See usage with no args.\n");
		return 1;
	}
	
	if (gui_mode) {
#ifdef GUI_SUPPORT
		for (int i = start_index; i < argc; i++) {
			// Skip option arguments
			if (strcmp(argv[i], "-g") == 0 || 
				strcmp(argv[i], "-e") == 0 || 
				strcmp(argv[i], "--escapes") == 0) {
				continue;
			}
			if (insert_text_x11(argv[i], escape_mode) != 0) {
				return 1;
			}
			// Add space between arguments except after the last one
			if (i < argc - 1) {
				insert_text_x11(" ", 0); // No need to process escape in a space
			}
		}
#else
		fprintf(stderr, "GUI support not compiled in\n");
		return 1;
#endif
	} else {
		return insert_text_tty(argc, argv, escape_mode);
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

// Function to convert escape sequences in strings
char* process_escape_sequences(const char* input, int* error) {
	if (!input) return NULL;
	
	size_t len = strlen(input);
	char* output = malloc(len + 1); // At most, output will be same length as input
	if (!output) {
		*error = ENOMEM;
		return NULL;
	}
	
	size_t i = 0, j = 0;
	while (i < len) {
		if (input[i] == '\\' && i + 1 < len) {
			if (input[i+1] == '\\') {
				// Literal backslash
				output[j++] = '\\';
				i += 2;
			} else if (input[i+1] == '^' && i + 2 < len) {
				// Control character sequence \^c
				char ctrl_char = toupper(input[i+2]); // Convert to uppercase for consistency
				if (ctrl_char >= '@' && ctrl_char <= '_') {
					// Valid control character range (@ = 0x40, _ = 0x5F)
					// Control character value is ASCII value - 64 (0x40)
					output[j++] = ctrl_char - 64;
					i += 3;
				} else {
					fprintf(stderr, "Error: Invalid control character '\\^%c'\n", input[i+2]);
					*error = EINVAL;
					free(output);
					return NULL;
				}
			} else if (input[i+1] == 'x' && i + 3 < len && 
					  isxdigit((unsigned char)input[i+2]) && 
					  isxdigit((unsigned char)input[i+3])) {
				// Hex escape sequence \xhh
				char hex[3] = {input[i+2], input[i+3], '\0'};
				int value;
				if (sscanf(hex, "%x", &value) != 1) {
					fprintf(stderr, "Error: Invalid hex escape sequence '\\x%s'\n", hex);
					*error = EINVAL;
					free(output);
					return NULL;
				}
				output[j++] = (char)value;
				i += 4;
			} else if (input[i+1] >= '0' && input[i+1] <= '7') {
				// Octal escape sequence \ooo (up to 3 digits)
				int value = 0;
				int count = 0;
				i++; // Skip the backslash
				while (count < 3 && i < len && input[i] >= '0' && input[i] <= '7') {
					value = value * 8 + (input[i] - '0');
					i++;
					count++;
				}
				if (value > 255) {
					fprintf(stderr, "Error: Octal value \\%03o out of range\n", value);
					*error = EINVAL;
					free(output);
					return NULL;
				}
				output[j++] = (char)value;
			} else {
				// Invalid escape sequence, treat \ as literal
				fprintf(stderr, "Warning: Unknown escape sequence '\\%c', treating as literal characters\n", input[i+1]);
				output[j++] = input[i++];
				output[j++] = input[i++];
			}
		} else {
			// Normal character
			output[j++] = input[i++];
		}
	}
	
	output[j] = '\0';
	*error = 0;
	return output;
}

int insert_text_tty(int argc, char *argv[], int escape_mode) {
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
	
	int result = 0;
	int first_arg = 1;
	
	// Skip option flags
	while (first_arg < argc && 
	      (strcmp(argv[first_arg], "-g") == 0 || 
	       strcmp(argv[first_arg], "-e") == 0 || 
	       strcmp(argv[first_arg], "--escapes") == 0)) {
		first_arg++;
	}
	
	for (size_t ai = first_arg; ai < argc; ai++) {
		const char *s = argv[ai];
		
		// Skip option flags that might be in the middle
		if (strcmp(s, "-g") == 0 || strcmp(s, "-e") == 0 || strcmp(s, "--escapes") == 0) {
			continue;
		}
		
		// Add space between arguments except before the first one
		if (ai > first_arg) {
			if (ioctl(fd, TIOCSTI, " ") == -1) {
				perror("ioctl");
				result = 1;
				break;
			}
		}
		
		if (!escape_mode) {
			// No escape processing - original behavior
			size_t slen = strlen(s);
			for (size_t i = 0; i < slen; i++) {
				if (ioctl(fd, TIOCSTI, s+i) == -1) {
					perror("ioctl");
					result = 1;
					break;
				}
			}
			if (result != 0) break;
		} else {
			// Process escape sequences
			int error = 0;
			char* processed = process_escape_sequences(s, &error);
			
			if (error != 0) {
				result = error;
				break;
			}
			
			size_t slen = strlen(processed);
			for (size_t i = 0; i < slen; i++) {
				if (ioctl(fd, TIOCSTI, processed+i) == -1) {
					perror("ioctl");
					free(processed);
					result = 1;
					break;
				}
			}
			
			free(processed);
			if (result != 0) break;
		}
	}

	if (restore_echo(fd, &original_termios)) {
		close(fd);
		return 1;
	}

	close(fd);
	return result;
}

#ifdef GUI_SUPPORT
int insert_text_x11(const char* text, int escape_mode) {
    // Process escape sequences if needed
    char* processed_text = NULL;
    if (escape_mode) {
        int error = 0;
        processed_text = process_escape_sequences(text, &error);
        if (error != 0) {
            return error;
        }
        text = processed_text;  // Use the processed text
    }
    
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        if (processed_text) free(processed_text);
        return 1;
    }

    // Retrieve the range of keycodes used by the keyboard
    int min_keycode, max_keycode;
    XDisplayKeycodes(display, &min_keycode, &max_keycode);

    // Get the keyboard mapping
    int keysyms_per_keycode;
    KeySym *keymap = XGetKeyboardMapping(display, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

    // Simulate keypresses for each character in the string
    for (const char *p = text; *p != '\0'; p++) {
        KeySym ks = XStringToKeysym((char[]){*p, 0});

        // If XStringToKeysym fails, search the keymap
        if (ks == NoSymbol) {
            for (int i = 0; i < (max_keycode - min_keycode + 1) * keysyms_per_keycode; ++i) {
                if (keymap[i] == *p) {
                    ks = keymap[i];
                    break;
                }
            }
        }

        if (ks == NoSymbol) {
            fprintf(stderr, "No symbol for %c\n", *p);
            continue;
        }

        // Search for the keysym in the keyboard mapping
        KeyCode keycode = 0;
        int shift_pressed = 0;
        for (int kc = min_keycode; kc <= max_keycode; ++kc) {
            for (int i = 0; i < keysyms_per_keycode; ++i) {
                if (keymap[(kc - min_keycode) * keysyms_per_keycode + i] == ks) {
                    keycode = kc;
                    shift_pressed = (i % 2 == 1); // Assumes shift is the only modifier
                    goto found;
                }
            }
        }
        
    found:
        if (!keycode) {
            fprintf(stderr, "No keycode found for symbol %lu\n", (unsigned long)ks);
            continue;
        }

        // Press Shift if needed
        if (shift_pressed) {
            KeyCode shift_keycode = XKeysymToKeycode(display, XK_Shift_L);
            XTestFakeKeyEvent(display, shift_keycode, True, 0);
        }

        // Simulate keypress
        XTestFakeKeyEvent(display, keycode, True, 0);  // key press
        XTestFakeKeyEvent(display, keycode, False, 0); // key release

        // Release Shift if it was pressed
        if (shift_pressed) {
            KeyCode shift_keycode = XKeysymToKeycode(display, XK_Shift_L);
            XTestFakeKeyEvent(display, shift_keycode, False, 0);
        }

        // Flush the output buffer
        XFlush(display);
    }

    XFree(keymap);
    XCloseDisplay(display);
    
    if (processed_text) free(processed_text);
    return 0;
}
#endif

// kinject.c — inject keystrokes anywhere via /dev/uinput
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

static int ufd = -1;

// Emit one input_event
static int emit(int type, int code, int val) {
    struct input_event ie = {0};
    ie.type = type;
    ie.code = code;
    ie.value = val;
    // timestamp will be filled by the kernel if zero
    if (write(ufd, &ie, sizeof(ie)) != sizeof(ie)) {
        perror("write");
        return -1;
    }
    return 0;
}

// near top of file, after your includes:
static const int keycodes_alpha[26] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
    KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
    KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y,
    KEY_Z
};

static int char_to_keycode(char c, int *needs_shift) {
    *needs_shift = 0;

    // lowercase
    if (c >= 'a' && c <= 'z') {
        return keycodes_alpha[c - 'a'];
    }
    // uppercase
    if (c >= 'A' && c <= 'Z') {
        *needs_shift = 1;
        return keycodes_alpha[c - 'A'];
    }
    // digits 1–9 are consecutive in uinput; 0 is after 9
    if (c >= '1' && c <= '9') {
        return KEY_1 + (c - '1');
    }
    if (c == '0') {
        return KEY_0;
    }

    // space and enter
    if (c == ' ')   return KEY_SPACE;
    if (c == '\n' || c == '\r') return KEY_ENTER;

    // common punctuation and shifted variants
    switch (c) {
      // unshifted
      case '-': return KEY_MINUS;
      case '=': return KEY_EQUAL;
      case '[': return KEY_LEFTBRACE;
      case ']': return KEY_RIGHTBRACE;
      case '\\':return KEY_BACKSLASH;
      case ';': return KEY_SEMICOLON;
      case '\'':return KEY_APOSTROPHE;
      case ',': return KEY_COMMA;
      case '.': return KEY_DOT;
      case '/': return KEY_SLASH;
      case '`': return KEY_GRAVE;
      // shifted (set needs_shift = 1)
      case '!': *needs_shift = 1; return KEY_1;
      case '@': *needs_shift = 1; return KEY_2;
      case '#': *needs_shift = 1; return KEY_3;
      case '$': *needs_shift = 1; return KEY_4;
      case '%': *needs_shift = 1; return KEY_5;
      case '^': *needs_shift = 1; return KEY_6;
      case '&': *needs_shift = 1; return KEY_7;
      case '*': *needs_shift = 1; return KEY_8;
      case '(': *needs_shift = 1; return KEY_9;
      case ')': *needs_shift = 1; return KEY_0;
      case '_': *needs_shift = 1; return KEY_MINUS;
      case '+': *needs_shift = 1; return KEY_EQUAL;
      case '{': *needs_shift = 1; return KEY_LEFTBRACE;
      case '}': *needs_shift = 1; return KEY_RIGHTBRACE;
      case '|': *needs_shift = 1; return KEY_BACKSLASH;
      case ':': *needs_shift = 1; return KEY_SEMICOLON;
      case '"': *needs_shift = 1; return KEY_APOSTROPHE;
      case '<': *needs_shift = 1; return KEY_COMMA;
      case '>': *needs_shift = 1; return KEY_DOT;
      case '?': *needs_shift = 1; return KEY_SLASH;
      case '~': *needs_shift = 1; return KEY_GRAVE;
    }

    // everything else unsupported
    return -1;
}

// Set up the uinput device
static int setup_uinput(void) {
    ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) { perror("open /dev/uinput"); return -1; }

    // Enable key events and all keys we might need
    if (ioctl(ufd, UI_SET_EVBIT, EV_KEY) < 0) return -1;
    for (int k = KEY_A; k <= KEY_Z; k++)
        ioctl(ufd, UI_SET_KEYBIT, k);
    for (int k = KEY_1; k <= KEY_0; k++)
        ioctl(ufd, UI_SET_KEYBIT, k);
    // punctuation & others
    int extra[] = {KEY_SPACE, KEY_ENTER, KEY_MINUS, KEY_EQUAL,
                   KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH,
                   KEY_SEMICOLON, KEY_APOSTROPHE, KEY_COMMA,
                   KEY_DOT, KEY_SLASH, KEY_GRAVE,
                   KEY_LEFTSHIFT
                  };
    for (size_t i = 0; i < sizeof(extra)/sizeof(*extra); i++)
        ioctl(ufd, UI_SET_KEYBIT, extra[i]);

    // Create the device
    struct uinput_setup usetup = {0};
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "kinject-uinput");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1234;
    usetup.id.product = 0x5678;
    if (ioctl(ufd, UI_DEV_SETUP, &usetup) < 0) { perror("UI_DEV_SETUP"); return -1; }
    if (ioctl(ufd, UI_DEV_CREATE)  < 0) { perror("UI_DEV_CREATE"); return -1; }

    // Minimum delay to let the device settle
    sleep(1);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <string to inject>\n", argv[0]);
        return 1;
    }
    if (setup_uinput() < 0)
        return 1;

    // Concatenate all args with spaces
    size_t total = 0;
    for (int i = 1; i < argc; i++)
        total += strlen(argv[i]) + 1;
    char *s = malloc(total);
    s[0] = 0;
    for (int i = 1; i < argc; i++) {
        strcat(s, argv[i]);
        if (i+1 < argc) strcat(s, " ");
    }

    // Inject each character
    for (char *p = s; *p; p++) {
        int shift = 0, code = char_to_keycode(*p, &shift);
        if (code < 0) {
            fprintf(stderr, "Warning: no keycode for '%c'\n", *p);
            continue;
        }
        if (shift) {
            emit(EV_KEY, KEY_LEFTSHIFT, 1);
            emit(EV_SYN, SYN_REPORT, 0);
        }
        emit(EV_KEY, code, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        emit(EV_KEY, code, 0);
        emit(EV_SYN, SYN_REPORT, 0);
        if (shift) {
            emit(EV_KEY, KEY_LEFTSHIFT, 0);
            emit(EV_SYN, SYN_REPORT, 0);
        }
        usleep(5000);  // small delay between keys
    }
    free(s);

    // Destroy
    ioctl(ufd, UI_DEV_DESTROY);
    close(ufd);
    return 0;
}

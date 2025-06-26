/* kinject.c — inject keystrokes via /dev/uinput, with optional escape processing (-e)
 * Usage: kinject [-e|--escapes] <string> [...]
 */
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

// Emit one input_event to uinput
static int emit(int type, int code, int val) {
    struct input_event ie = {0};
    ie.type = type;
    ie.code = code;
    ie.value = val;
    // kernel timestamps if zeros
    if (write(ufd, &ie, sizeof(ie)) != sizeof(ie)) {
        perror("write");
        return -1;
    }
    return 0;
}

// Simple escape-sequence processor: \\, \n, \r, \xhh, \0oo, \^C

static char* process_escapes(const char *in) {
    if (!in) return NULL;
    size_t len = strlen(in);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t i=0, j=0;
    while (i < len) {
        if (in[i] == '\\' && i+1 < len) {
            i++;
            if (in[i] == 'n')      { out[j++] = '\n'; i++; }
            else if (in[i] == 'r') { out[j++] = '\r'; i++; }
            else if (in[i] == '^' && i+1 < len) {
                char c = in[i+1];
                c = toupper((unsigned char)c);
                if (c >= '@' && c <= '_') {
                    out[j++] = c - 64;
                    i += 2;
                } else {
                    out[j++] = '^';
                    i++;
                }
            }
            else if (in[i]=='x' && i+2 < len && isxdigit(in[i+1]) && isxdigit(in[i+2])) {
                char hex[3] = {in[i+1], in[i+2], '\0'};
                int v; sscanf(hex, "%x", &v);
                out[j++] = v;
                i += 3;
            }
            else if (in[i] >= '0' && in[i] <= '7') {
                int v=0, count=0;
                while (count<3 && i<len && in[i]>='0'&&in[i]<='7') {
                    v = v*8 + (in[i]-'0');
                    i++; count++;
                }
                out[j++] = v;
            }
            else if (in[i] == '\\') {
                out[j++] = '\\'; i++;
            }
            else {
                // unknown, copy literally
                out[j++] = in[i++];
            }
        } else {
            out[j++] = in[i++];
        }
    }
    out[j] = '\0';
    return out;
}

// Map characters to uinput keycodes
static const int keycodes_alpha[26] = { KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z };
static int char_to_keycode(char c, int *shift) {
    *shift = 0;
    if (c>='a'&&c<='z') return keycodes_alpha[c-'a'];
    if (c>='A'&&c<='Z') { *shift=1; return keycodes_alpha[c-'A']; }
    if (c>='1'&&c<='9') return KEY_1 + (c-'1');
    if (c=='0')        return KEY_0;
    if (c==' ') return KEY_SPACE;
    if (c=='\n'||c=='\r') return KEY_ENTER;
    switch(c) {
        case '-': return KEY_MINUS;
        case '=': return KEY_EQUAL;
        case '[': return KEY_LEFTBRACE;
        case ']': return KEY_RIGHTBRACE;
        case '\\': return KEY_BACKSLASH;
        case ';': return KEY_SEMICOLON;
        case '\'': return KEY_APOSTROPHE;
        case ',': return KEY_COMMA;
        case '.': return KEY_DOT;
        case '/': return KEY_SLASH;
        case '`': return KEY_GRAVE;
        case '!': *shift=1; return KEY_1;
        case '@': *shift=1; return KEY_2;
        case '#': *shift=1; return KEY_3;
        case '$': *shift=1; return KEY_4;
        case '%': *shift=1; return KEY_5;
        case '^': *shift=1; return KEY_6;
        case '&': *shift=1; return KEY_7;
        case '*': *shift=1; return KEY_8;
        case '(': *shift=1; return KEY_9;
        case ')': *shift=1; return KEY_0;
        case '_': *shift=1; return KEY_MINUS;
        case '+': *shift=1; return KEY_EQUAL;
        case '{': *shift=1; return KEY_LEFTBRACE;
        case '}': *shift=1; return KEY_RIGHTBRACE;
        case '|': *shift=1; return KEY_BACKSLASH;
        case ':': *shift=1; return KEY_SEMICOLON;
        case '"': *shift=1; return KEY_APOSTROPHE;
        case '<': *shift=1; return KEY_COMMA;
        case '>': *shift=1; return KEY_DOT;
        case '?': *shift=1; return KEY_SLASH;
        case '~': *shift=1; return KEY_GRAVE;
    }
    return -1;
}

static int setup_uinput(void) {
    ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd<0) { perror("open /dev/uinput"); return -1; }
    ioctl(ufd, UI_SET_EVBIT, EV_KEY);
    for(int k=KEY_A;k<=KEY_Z;k++) ioctl(ufd, UI_SET_KEYBIT, k);
    for(int k=KEY_1;k<=KEY_0;k++) ioctl(ufd, UI_SET_KEYBIT, k);
    int extra[] = {KEY_SPACE,KEY_ENTER,KEY_MINUS,KEY_EQUAL,KEY_LEFTBRACE,KEY_RIGHTBRACE,KEY_BACKSLASH,KEY_SEMICOLON,KEY_APOSTROPHE,KEY_COMMA,KEY_DOT,KEY_SLASH,KEY_GRAVE,KEY_LEFTSHIFT};
    for(size_t i=0;i<sizeof(extra)/sizeof(extra[0]);i++) ioctl(ufd, UI_SET_KEYBIT, extra[i]);
    struct uinput_setup usetup={0};
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "kinject-uinput");
    usetup.id.bustype=BUS_USB; usetup.id.vendor=0x1234; usetup.id.product=0x5678;
    ioctl(ufd, UI_DEV_SETUP, &usetup);
    ioctl(ufd, UI_DEV_CREATE);
    sleep(1);
    return 0;
}

int main(int argc, char *argv[]) {
    int escape_mode = 0;
    int arg0 = 1;
    if (argc>1 && (strcmp(argv[1], "-e")==0 || strcmp(argv[1], "--escapes")==0)) {
        escape_mode = 1;
        arg0 = 2;
    }
    if (argc <= arg0) {
        fprintf(stderr, "Usage: %s [-e|--escapes] <text> [...]\n", argv[0]);
        return 1;
    }
    // build input string
    size_t tot=0;
    for(int i=arg0;i<argc;i++) tot+=strlen(argv[i])+1;
    char *raw = malloc(tot+1); raw[0]=0;
    for(int i=arg0;i<argc;i++) {
        strcat(raw, argv[i]);
        if (i<argc-1) strcat(raw, " ");
    }
    char *text = raw;
    if (escape_mode) {
        char *p = process_escapes(raw);
        free(raw);
        text = p;
        if (!text) return 1;
    }
    if (setup_uinput()<0) return 1;
    // inject
    for(char *p=text; *p; p++) {
        int shift=0, code = char_to_keycode(*p, &shift);
        if (code<0) continue;
        if (shift) { emit(EV_KEY, KEY_LEFTSHIFT,1); emit(EV_SYN,SYN_REPORT,0); }
        emit(EV_KEY, code,1); emit(EV_SYN,SYN_REPORT,0);
        emit(EV_KEY, code,0); emit(EV_SYN,SYN_REPORT,0);
        if (shift) { emit(EV_KEY, KEY_LEFTSHIFT,0); emit(EV_SYN,SYN_REPORT,0); }
        usleep(5000);
    }
    // cleanup
    ioctl(ufd, UI_DEV_DESTROY);
    close(ufd);
    if (escape_mode) free(text);
    return 0;
}

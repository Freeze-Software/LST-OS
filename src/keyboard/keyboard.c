#include "../kernel.h"

#define KB_BUF_SIZE 128
volatile char kb_buffer[KB_BUF_SIZE];
volatile int kb_head = 0;
volatile int kb_tail = 0;

static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0,'*',0,' ',0,
};

static const char shift_keymap[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,'|',
    'Z','X','C','V','B','N','M','<','>','?', 0,'*',0,' ',0,
};

int shift = 0;
static int set2_break = 0;
static int set2_shift = 0;
static int set1_e0 = 0;
static int set2_e0 = 0;

static char translate_scancode(unsigned char scancode) {
    char c;

    if (scancode == 0xE0) {
        set1_e0 = 1;
        return 0;
    }

    if (set1_e0) {
        set1_e0 = 0;

        if (scancode & 0x80) {
            return 0;
        }

        // Arrow keys in set 1: map to movement keys for launcher navigation.
        if (scancode == 0x48) return 'w';
        if (scancode == 0x50) return 's';
        if (scancode == 0x4B) return 'a';
        if (scancode == 0x4D) return 'd';
        return 0;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift = 1;
        return 0;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        shift = 0;
        return 0;
    }

    if (scancode & 0x80) {
        return 0;
    }

    if (scancode >= 128) {
        return 0;
    }

    if (shift) {
        c = shift_keymap[scancode];
    } else {
        c = keymap[scancode];
    }

    return c;
}

static char translate_scancode_set2(unsigned char scancode) {
    char c = 0;

    if (scancode == 0xE0) {
        set2_e0 = 1;
        return 0;
    }

    if (scancode == 0xF0) {
        set2_break = 1;
        return 0;
    }

    if (set2_e0) {
        if (set2_break) {
            set2_break = 0;
            set2_e0 = 0;
            return 0;
        }

        set2_e0 = 0;

        // Arrow keys in set 2 extended sequence.
        if (scancode == 0x75) return 'w';
        if (scancode == 0x72) return 's';
        if (scancode == 0x6B) return 'a';
        if (scancode == 0x74) return 'd';
        return 0;
    }

    if (scancode == 0x12 || scancode == 0x59) {
        if (set2_break) {
            set2_shift = 0;
            set2_break = 0;
        } else {
            set2_shift = 1;
        }
        return 0;
    }

    if (set2_break) {
        set2_break = 0;
        return 0;
    }

    switch (scancode) {
        case 0x76: c = 27; break;      // Esc
        case 0x66: c = '\b'; break;    // Backspace
        case 0x0D: c = '\t'; break;    // Tab
        case 0x5A: c = '\n'; break;    // Enter
        case 0x29: c = ' '; break;

        case 0x16: c = set2_shift ? '!' : '1'; break;
        case 0x1E: c = set2_shift ? '@' : '2'; break;
        case 0x26: c = set2_shift ? '#' : '3'; break;
        case 0x25: c = set2_shift ? '$' : '4'; break;
        case 0x2E: c = set2_shift ? '%' : '5'; break;
        case 0x36: c = set2_shift ? '^' : '6'; break;
        case 0x3D: c = set2_shift ? '&' : '7'; break;
        case 0x3E: c = set2_shift ? '*' : '8'; break;
        case 0x46: c = set2_shift ? '(' : '9'; break;
        case 0x45: c = set2_shift ? ')' : '0'; break;
        case 0x4E: c = set2_shift ? '_' : '-'; break;
        case 0x55: c = set2_shift ? '+' : '='; break;

        case 0x15: c = set2_shift ? 'Q' : 'q'; break;
        case 0x1D: c = set2_shift ? 'W' : 'w'; break;
        case 0x24: c = set2_shift ? 'E' : 'e'; break;
        case 0x2D: c = set2_shift ? 'R' : 'r'; break;
        case 0x2C: c = set2_shift ? 'T' : 't'; break;
        case 0x35: c = set2_shift ? 'Y' : 'y'; break;
        case 0x3C: c = set2_shift ? 'U' : 'u'; break;
        case 0x43: c = set2_shift ? 'I' : 'i'; break;
        case 0x44: c = set2_shift ? 'O' : 'o'; break;
        case 0x4D: c = set2_shift ? 'P' : 'p'; break;
        case 0x54: c = set2_shift ? '{' : '['; break;
        case 0x5B: c = set2_shift ? '}' : ']'; break;

        case 0x1C: c = set2_shift ? 'A' : 'a'; break;
        case 0x1B: c = set2_shift ? 'S' : 's'; break;
        case 0x23: c = set2_shift ? 'D' : 'd'; break;
        case 0x2B: c = set2_shift ? 'F' : 'f'; break;
        case 0x34: c = set2_shift ? 'G' : 'g'; break;
        case 0x33: c = set2_shift ? 'H' : 'h'; break;
        case 0x3B: c = set2_shift ? 'J' : 'j'; break;
        case 0x42: c = set2_shift ? 'K' : 'k'; break;
        case 0x4B: c = set2_shift ? 'L' : 'l'; break;
        case 0x4C: c = set2_shift ? ':' : ';'; break;
        case 0x52: c = set2_shift ? '"' : '\''; break;

        case 0x0E: c = set2_shift ? '~' : '`'; break;
        case 0x5D: c = set2_shift ? '|' : '\\'; break;
        case 0x1A: c = set2_shift ? 'Z' : 'z'; break;
        case 0x22: c = set2_shift ? 'X' : 'x'; break;
        case 0x21: c = set2_shift ? 'C' : 'c'; break;
        case 0x2A: c = set2_shift ? 'V' : 'v'; break;
        case 0x32: c = set2_shift ? 'B' : 'b'; break;
        case 0x31: c = set2_shift ? 'N' : 'n'; break;
        case 0x3A: c = set2_shift ? 'M' : 'm'; break;
        case 0x41: c = set2_shift ? '<' : ','; break;
        case 0x49: c = set2_shift ? '>' : '.'; break;
        case 0x4A: c = set2_shift ? '?' : '/'; break;
        default: c = 0; break;
    }

    return c;
}

static char translate_scancode_auto(unsigned char scancode) {
    char c = translate_scancode(scancode);
    if (c) {
        return c;
    }

    return translate_scancode_set2(scancode);
}

int keyboard_has_char(void) {
    return inb(0x64) & 1;
}

void keyboard_handler_c() {
    if (inb(0x64) & 0x20) {
        return;
    }

    unsigned char scancode = inb(0x60);

    char c = translate_scancode_auto(scancode);

    if (!c) return;

    int next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

int kb_available(void) {
    return kb_head != kb_tail;
}

char keyboard_poll_char(void) {
    uint8_t status = inb(0x64);

    if (!(status & 1) || (status & 0x20)) {
        return 0;
    }

    return translate_scancode_auto(inb(0x60));
}

char getchar(void) {
    if (kb_head == kb_tail) {
        return 0;
    }

    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;

    return c;
}

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

int keyboard_has_char(void) {
    return inb(0x64) & 1;
}

void keyboard_handler_c() {
    if (inb(0x64) & 0x20)
        return;
    
    unsigned char scancode = inb(0x60);

    // SHIFT press
    if (scancode == 0x2A || scancode == 0x36) {
        shift = 1;
        return;
    }

    // SHIFT release
    if (scancode == 0xAA || scancode == 0xB6) {
        shift = 0;
        return;
    }

    if (scancode & 0x80) {
        return;
    }

    if (scancode >= 128) return;

    char c;

    if (shift) {
        c = shift_keymap[scancode];
    } else {
        c = keymap[scancode];
    }

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

char getchar(void) {
    if (kb_head == kb_tail) {
        return 0;
    }

    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;

    return c;
}

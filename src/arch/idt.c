#include <stdint.h>

/* =========================
   IDT STRUCTURES
========================= */

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

/* =========================
   ASM FUNCTIONS
========================= */

extern void idt_load(void* idt_ptr);
extern void irq1_handler();
extern void timer_stub();
extern void irq12_mouse_stub();

/* =========================
   PORT I/O (PIC)
========================= */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* =========================
   IDT SETUP
========================= */

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;

    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

/* =========================
   DEFAULT INTERRUPT HANDLER
========================= */

void isr_handler() {
    while (1) {
        __asm__("hlt");
    }
}

/* =========================
   PIC REMAP
========================= */

void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xF8); // Master PIC: IRQ0 (timer), IRQ1 (keyboard), IRQ2 (cascade) enabled
    outb(0xA1, 0xEF); // Slave PIC: IRQ12 (mouse) enabled
}

/* =========================
   IDT INIT
========================= */

extern void timer_stub(void);

void idt_init() {
    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base  = (uint32_t)&idt;
    pic_remap();

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_handler, 0x08, 0x8E);
    }

    idt_set_gate(0x21, (uint32_t)irq1_handler, 0x08, 0x8E);
    idt_set_gate(0x20, (uint32_t)timer_stub, 0x08, 0x8E);
    idt_set_gate(0x2C, (uint32_t)irq12_mouse_stub, 0x08, 0x8E);

    idt_load(&idtp);
}

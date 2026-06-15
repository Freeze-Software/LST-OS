global idt_load
extern keyboard_handler_c

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

global irq1_handler

irq1_handler:
    pusha
    call keyboard_handler_c
    mov al, 0x20
    out 0x20, al
    popa
    iretd

global irq12_mouse_stub
extern mouse_irq_handler

irq12_mouse_stub:
    pusha
    call mouse_irq_handler
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    popa
    iret

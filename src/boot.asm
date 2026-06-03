global _start
extern kernel_main
extern _bss_start
extern _bss_end

section .multiboot
align 8

header_start:
    dd 0xe85250d6                ; magic
    dd 0                         ; architecture
    dd header_end - header_start
    dd -(0xe85250d6 + 0 + (header_end - header_start))
    ; Framebuffer Request Tag
    align 8
    dw 5                         ; type = framebuffer
    dw 0                         ; flags
    dd 20                        ; size
    dd 1920                      ; width
    dd 1080                      ; height
    dd 32                        ; bpp
    ; End Tag
    align 8
    dw 0
    dw 0
    dd 8

header_end:

section .bootstrap_stack nobits alloc write
align 16
stack_bottom:
    resb 1048576
stack_top:

section .text
_start:
    mov  esp, stack_top
    push ebx             ; multiboot info pointer
    push eax             ; multiboot magic
    mov  edi, _bss_start
    mov  ecx, _bss_end
    sub  ecx, edi
    shr  ecx, 2
    xor  eax, eax
    rep  stosd
    ; kernel_main(magic, mb_addr)
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang

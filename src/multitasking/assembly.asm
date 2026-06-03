global timer_stub
extern schedule
extern current_task
extern tasks

timer_stub:
    cli
    pusha
    mov eax, [current_task]
    imul eax, 44
    add eax, tasks
    mov [eax + 8], esp
    call schedule
    mov eax, [current_task]
    imul eax, 44
    add eax, tasks
    mov esp, [eax + 8]
    popa
    mov al, 0x20
    out 0x20, al
    sti
    iret

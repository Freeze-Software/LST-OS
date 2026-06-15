global timer_stub
extern schedule
extern current_task
extern tasks
extern pit_on_tick

timer_stub:
    cli
    pusha
    call pit_on_tick
    mov eax, [current_task]
    cmp eax, 0
    jl .pick_next
    imul eax, 44
    add eax, tasks
    mov [eax + 8], esp
.pick_next:
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

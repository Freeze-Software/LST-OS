#include <stdint.h> 
// I use arch btw
void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

void get_cpu_name(char* buf) {
    uint32_t eax, ebx, ecx, edx;
    
    // Leaf 0x80000002 - 0x80000004 = CPU Brand String
    for (int i = 0; i < 3; i++) {
        cpuid(0x80000002 + i, &eax, &ebx, &ecx, &edx);
        *(uint32_t*)(buf + i*16 +  0) = eax;
        *(uint32_t*)(buf + i*16 +  4) = ebx;
        *(uint32_t*)(buf + i*16 +  8) = ecx;
        *(uint32_t*)(buf + i*16 + 12) = edx;
    }
    buf[48] = 0;
}

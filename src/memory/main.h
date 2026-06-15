//Simple header for main .c file
#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (1024 * 10240)
#define ALIGNMENT 8

typedef struct block {
    size_t size;
    int free;
    struct block* next;
} __attribute__((aligned(8))) block_t;

void init_heap(void);

void* malloc(size_t size);
void  free(void* ptr);
void* realloc(void* ptr, size_t size);

#endif // MEMORY_H

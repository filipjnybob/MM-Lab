#include <stdlib.h>
#include <stdbool.h>

#define ALIGNMENT 16 /* The alignment of all payloads returned by umalloc */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

/*
 * memory_block_t - Represents a block of memory managed by the heap. The 
 * struct can be left as is, or modified for your design.
 * In the current design bit0 is the allocated bit
 * bits 1-3 are unused.
 * and the remaining 60 bit represent the size.
 * The struct has been modified adding the magic_number and prev fields.
 * magic_number stores a unique number used to validate the block.
 * prev stores a pointer to the previous block in the free list.
 */
typedef struct memory_block_struct
{
    size_t block_size_alloc;
    int magic_number;
    struct memory_block_struct *prev;
    struct memory_block_struct *next;
} memory_block_t;

typedef struct footer
{
    size_t block_size_alloc;
    int magic_number;
} footer_t;

// Helper Functions, this may be editted if you change the signature in umalloc.c
bool is_allocated(memory_block_t *block);
bool is_allocated_footer(footer_t *block);
void allocate(memory_block_t *block);
void deallocate(memory_block_t *block);
size_t get_size(memory_block_t *block);
size_t get_size_footer(footer_t *footer);
memory_block_t *get_next(memory_block_t *block);
void put_block(memory_block_t *block, size_t size, bool alloc);
void *get_payload(memory_block_t *block);
memory_block_t *get_block(void *payload);
footer_t *get_footer(memory_block_t *block);
memory_block_t *get_header(footer_t *footer);

void insert(memory_block_t *block);
void insert_at_end(memory_block_t *block);
void remove_from_list(memory_block_t *block);
memory_block_t *get_above_header(memory_block_t *block);
footer_t *get_below_footer(memory_block_t *block);

memory_block_t *find(size_t size);
memory_block_t *extend(size_t size);
memory_block_t *split(memory_block_t *block, size_t size);
bool contained_in_block(void *ptr);
memory_block_t *coalesce(memory_block_t *block);

size_t get_padded_size(size_t size);
size_t get_block_size(size_t size);

// Portion that may not be edited
int uinit();
void *umalloc(size_t size);
void ufree(void *ptr);
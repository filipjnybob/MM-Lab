#include "umalloc.h"
#include "csbrk.h"
#include "ansicolors.h"
#include <stdio.h>
#include <assert.h>

const char author[] = ANSI_BOLD ANSI_COLOR_RED "Isaac Adams EID: iga263" ANSI_RESET;

const int MAGIC_NUMBER = 0x12345678;

const int HEADER_SIZE = sizeof(memory_block_t) + sizeof(footer_t);

extern int check_heap();

/*
 * The following helpers can be used to interact with the memory_block_t
 * struct, they can be adjusted as necessary.
 */

// A sample pointer to the start of the free list.
memory_block_t *free_head;

/*
 * is_allocated - Given a header, returns true if a block is marked as allocated.
 */
bool is_allocated(memory_block_t *block) {
    assert(block != NULL);
    return block->block_size_alloc & 0x1;
}

/*
 * is_allocated_footer - Given a footer, returns true if a block is marked as allocated.
 */
bool is_allocated_footer(footer_t *block) {
    assert(block != NULL);
    return block->block_size_alloc & 0x1;
}

/*
 * allocate - marks a block as allocated.
 */
void allocate(memory_block_t *block) {
    assert(block != NULL);
    block->block_size_alloc |= 0x1;
    footer_t* footer = get_footer(block);
    assert(footer != NULL);
    footer->block_size_alloc = block->block_size_alloc;
}


/*
 * deallocate - marks a block as unallocated.
 */
void deallocate(memory_block_t *block) {
    assert(block != NULL);
    block->block_size_alloc &= ~0x1;
    footer_t* footer = get_footer(block);
    assert(footer != NULL);
    footer->block_size_alloc = block->block_size_alloc;
}

/*
 * get_size - gets the size of the block.
 */
size_t get_size(memory_block_t *block) {
    assert(block != NULL);
    return block->block_size_alloc & ~(ALIGNMENT-1);
}

/*
 * get_size_footer - Given a footer, returns the size of the block
 */
size_t get_size_footer(footer_t *footer) {
    assert(footer != NULL);
    return footer->block_size_alloc & ~(ALIGNMENT-1);
}

/*
 * get_next - gets the next block.
 */
memory_block_t *get_next(memory_block_t *block) {
    assert(block != NULL);
    return block->next;
}

/*
 * put_block - puts a block struct into memory at the specified address.
 * Initializes the size, allocated, and magic number fields, along with NUlling out the next 
 * field.
 */
void put_block(memory_block_t *block, size_t size, bool alloc) {
    assert(block != NULL);
    assert(size % ALIGNMENT == 0);
    assert(alloc >> 1 == 0);
    block->block_size_alloc = size | alloc;
    block->magic_number = MAGIC_NUMBER;
    block->prev = NULL;
    block->next = NULL;

    // put footer
    footer_t* footer = get_footer(block);
    footer->block_size_alloc = block->block_size_alloc;
    footer->magic_number = MAGIC_NUMBER;
}

/*
 * get_payload - gets the payload of the block.
 */
void *get_payload(memory_block_t *block) {
    assert(block != NULL);
    return (void*)(block + 1);
}

/*
 * get_block - given a payload, returns the block.
 */
memory_block_t *get_block(void *payload) {
    assert(payload != NULL);
    return ((memory_block_t *)payload) - 1;
}

/*
 * get_footer - given a header, gets the footer of the block.
 */
footer_t *get_footer(memory_block_t *block) {
    assert(block != NULL);
    return ((void*)(block + 1)) + get_size(block);
}

/*
 * get_header - given a footer, gets the header of the block.
 */
memory_block_t *get_header(footer_t* footer) {
    assert(footer != NULL);
    return ((memory_block_t*)(((void*)footer) - get_size_footer(footer))) - 1;
}

/*
 * The following are helper functions that can be implemented to assist in your
 * design, but they are not required. 
 */

/*
 * insert - inserts the given free block into the free list in order of increasing size.
 */
void insert(memory_block_t* block) {
    // Check if list is empty
    if(free_head == NULL) {
        free_head = block;
        free_head->next = free_head;
        free_head->prev = free_head;
    } else if(get_size(block) <= get_size(free_head)) {
        // Insert block at start
        block->next = free_head;
        block->prev = free_head->prev;
        free_head->prev->next = block;
        free_head->prev = block;
        free_head = block;
    } else {
        // Search through free list for proper position
        memory_block_t* previous = free_head;
        memory_block_t* current = get_next(free_head);
        while(current != free_head && get_size(block) > get_size(current)) {
            previous = current;
            current = get_next(current);
        }
        block->next = current;
        block->prev = previous;
        previous->next = block;
        current->prev = block;
    }
}

/*
 * insert_at_end - inserts given free block at the end of the free list
 */
void insert_at_end(memory_block_t* block) {
    if(free_head == NULL) {
        block->next = block;
        block->prev = block;
        free_head = block;
    } else {
        block->next = free_head;
        block->prev = free_head->prev;
        free_head->prev->next = block;
        free_head->prev = block;
    }
}

/*
 * remove_from_list - removes the given block from the free list if it is present
 */
void remove_from_list(memory_block_t* block) {
    if(block->next != NULL && block->prev != NULL) {
        if(block->next == block) {
            // block is the only element in the free list
            free_head = NULL;
            block->prev = NULL;
            block->next = NULL;
        } else {
            block->prev->next = block->next;
            block->next->prev = block->prev;
            if(free_head == block) {
                // Update free_head
                free_head = block->next;
            }
            block->prev = NULL;
            block->next = NULL;
        }
        
    }
}

/*
 * get_above_header - returns a pointer to the header of the block above the given block
 */
memory_block_t *get_above_header(memory_block_t* block) {
    return (memory_block_t*)(get_footer(block) + 1);
}

/*
 * get_below_footer - returns a pointer to the footer of the block below the given block
 */
footer_t *get_below_footer(memory_block_t* block) {
    return ((footer_t*) block) - 1;
}

/*
 * find - finds a free block that can satisfy the umalloc request. Free block found with best fit.
 * Free list should be sorted in increasing size. Extends the heap if not enough space is available.
 */
memory_block_t *find(size_t size) {

    if(get_size(free_head) >= size) {
        return free_head;
    }

    memory_block_t* current = get_next(free_head);
    while(current != free_head && get_size(current) < size) {
        current = get_next(current);
    }
    if(current == free_head) {
        current = extend(size);
    }
    return current;
}

/*
 * extend - extends the heap if more memory is required. Returns NULL if an error occurred.
 */
memory_block_t *extend(size_t size) {
    size_t extend_size = size * 2;
    void *result = csbrk(get_block_size(extend_size));
    if(result == NULL) {
        return NULL;
    }
    put_block(result, get_padded_size(size * 2), false);
    // This method should only be called if there isn't a free block big enough for the request
    // As such, the newly allocated block should be the biggest.
    insert_at_end(result);
    return result;
}

/*
 * split - splits a given block in parts, one allocated, one free.
 * Removes the allocated block from the free list while inserting the free block.
 * Returns the allocated block.
 */
memory_block_t *split(memory_block_t *block, size_t size) {
    size_t remaining_size = get_size(block) - get_padded_size(size);
    // Check that there's enough remaining size to split the block
    if(remaining_size < (HEADER_SIZE + ALIGNMENT)) {
        // Not enough remaining space, don't split
        allocate(block);
        remove_from_list(block);
        return block;
    }
    // Create the allocated block
    remove_from_list(block);
    put_block(block, get_padded_size(size), true);
    // Create the free block
    memory_block_t* free = get_above_header(block);
    put_block(free, remaining_size - HEADER_SIZE, false);
    insert(free);

    return block;
}

/*
 * coalesce - coalesces a free memory block with neighbors.
 */
memory_block_t *coalesce(memory_block_t *block) {
    
    // Check above
    memory_block_t* above = get_above_header(block);
    // Check magic number
    if(above != NULL && above->magic_number == MAGIC_NUMBER) {
        if(!is_allocated(above)) {
            remove_from_list(block);
            remove_from_list(above);
            size_t combined_size = get_size(block) + get_size(above) + HEADER_SIZE;
            put_block(block, combined_size, false);
        }
    }
    
    // Check below
    footer_t* below = get_below_footer(block);
    assert(below == get_footer(get_header(below)));
    assert(get_above_header(get_header(below)) == block);
    // Check magic number
    if(below != NULL && below->magic_number == MAGIC_NUMBER) {
        if(!is_allocated_footer(below)) {
            memory_block_t* below_head = get_header(below);
            assert(below_head->block_size_alloc == below->block_size_alloc);
            assert(below_head->magic_number == MAGIC_NUMBER);
            remove_from_list(block);
            remove_from_list(below_head);
            size_t combined_size = get_size(block) + get_size(below_head) + HEADER_SIZE;
            put_block(below_head, combined_size, false);
            block = below_head;
            assert(below_head->block_size_alloc == get_footer(below_head)->block_size_alloc);
        }
    }

    return block;
}

/*
 * get_padded_size - returns size with padding so that it will mainain alignment
 */

size_t get_padded_size(size_t size) {
    return size + (ALIGNMENT - (size % ALIGNMENT));
}

/*
 * get_block_size - returns the total size needed to store a block with the given size
 */
size_t get_block_size(size_t size) {
    return get_padded_size(size) + HEADER_SIZE;
}

/*
 * uinit - Used initialize metadata required to manage the heap
 * along with allocating initial memory.
 */
int uinit() {
    size_t initial_size = (ALIGNMENT * 5);
    memory_block_t* result = extend(initial_size);
    if(result == NULL) {
        return -1;
    }
    return 0;
}

/*
 * umalloc -  allocates size bytes and returns a pointer to the allocated memory.
 * Returns NULL if an error occurs.
 */
void *umalloc(size_t size) {
    memory_block_t* block = find(size);
    if(block == NULL) {
        return block;
    }
    block = split(block, size);
    return get_payload(block);
}

/*
 * ufree -  frees the memory space pointed to by ptr, which must have been called
 * by a previous call to malloc.
 */
void ufree(void *ptr) {
    memory_block_t* block = get_block(ptr);
    if(block != NULL) {
        // Check magic number
        if(block->magic_number == MAGIC_NUMBER) {
            deallocate(block);
            //coalesce(block);
            insert(block);
            //printf("Check heap Return: %d\n", check_heap());
        }
    }
}
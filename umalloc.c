#include "umalloc.h"
#include "csbrk.h"
#include "ansicolors.h"
#include <stdio.h>
#include <assert.h>

const char author[] = ANSI_BOLD ANSI_COLOR_RED "Isaac Adams EID: iga263" ANSI_RESET;

const int MAGIC_NUMBER = 0x12345678;

const int HEADER_SIZE = sizeof(memory_block_t) + sizeof(footer_t);

extern int check_heap();

int sbrk_block_size;

sbrk_block *block_head;

int heap_size = 0;

int extend_count = 0;

int list_size = 0;

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
    list_size++;
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
    list_size++;
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

void print_list() {
    memory_block_t* temp = free_head;
    do {
        printf("Address: %p, ", temp);
        temp = temp->next;
    } while(temp != free_head);
    printf("\n");
}

/*
 * remove_from_list - removes the given block from the free list if it is present
 */
void remove_from_list(memory_block_t* block) {
    if(block->next != NULL && block->prev != NULL) {
        list_size--;
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
            assert(block->prev->next != block && block->next->prev != block);
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

    if(free_head == NULL) {
        return extend(size);
    }

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
 * put_sbrk_block - Initializes a sbrk_block at the given address with the given size
 */
void put_sbrk_block(sbrk_block* block, size_t size) {
    block->sbrk_start = (uint64_t) (((void*)block) + get_padded_size(sizeof(sbrk_block)));
    block->sbrk_end = (uint64_t) ((void*)block->sbrk_start) + size;
    block->next = block_head;
    block_head = block;
}

/*
 * extend - extends the heap if more memory is required. Returns NULL if an error occurred.
 */
memory_block_t *extend(size_t size) {
    size_t extend_size = get_padded_size(heap_size * 1.5 + size);
    // Ensure extension is not too large
    if(extend_size > ALIGNMENT * PAGESIZE - HEADER_SIZE - sbrk_block_size) {
        extend_size = ALIGNMENT * PAGESIZE - HEADER_SIZE - sbrk_block_size;
    }
    heap_size += extend_size;
    void *result = csbrk(get_block_size(extend_size) + sbrk_block_size);
    if(result == NULL) {
        return NULL;
    }
    
    put_sbrk_block(result, get_block_size(extend_size));
    result = (void*)block_head->sbrk_start;
    put_block(result, extend_size, false);
    // This method should only be called if there isn't a free block big enough for the request
    // As such, the newly allocated block should be the biggest.
    insert_at_end(result);
    extend_count++;
    return result;
}

/*
 * split - splits a given block in parts, one allocated, one free.
 * Removes the allocated block from the free list while inserting the free block.
 * Returns the allocated block.
 */
memory_block_t *split(memory_block_t *block, size_t size) {
    remove_from_list(block);
    assert(get_size(block) % 16 == 0);
    assert(get_block_size(get_size(block)) == get_size(block) + HEADER_SIZE);
       int initial = get_block_size(get_size(block));
    size_t remaining_size = get_size(block) - get_padded_size(size);
    // Check that there's enough remaining size to split the block
    if(remaining_size < (HEADER_SIZE + ALIGNMENT)) {
        // Not enough remaining space, don't split
        allocate(block);
        return block;
    }
    // Create the allocated block
    put_block(block, get_padded_size(size), true);
    // Create the free block
    memory_block_t* free = get_above_header(block);
    put_block(free, remaining_size - HEADER_SIZE, false);
    assert(get_size(block) + get_size(free) + (HEADER_SIZE * 2) == initial);
    insert(free);

    return block;
}

/*
 * contained_in_block - Returns true if the given pointer lies in one of
 * the allocated blocks given by csbrk.
 */
bool contained_in_block(void* ptr) {
    sbrk_block* temp = block_head;
    uint64_t address = (uint64_t) ptr;
    while(temp != NULL) {
        if(address >= temp->sbrk_start && address < temp->sbrk_end) {
            return true;
        }
        temp = temp->next;
        //printf("%p\n", temp);
    }

    return false;
}

/*
 * coalesce - coalesces a free memory block with neighbors.
 */
memory_block_t *coalesce(memory_block_t *block) {
    printf("list_size = %d\n", list_size);
    if(!contained_in_block((void*)block) || !(block->magic_number == MAGIC_NUMBER) 
        || is_allocated(block)) {
            return block;
        }

    // Check above
    memory_block_t* above = get_above_header(block);
    // Check magic number
    if(contained_in_block((void*)above) && above->magic_number == MAGIC_NUMBER) {
        if(!is_allocated(above)) {
            remove_from_list(block);
            remove_from_list(above);
            size_t combined_size = get_size(block) + get_size(above) + HEADER_SIZE;
            put_block(block, combined_size, false);
            printf("Coalesced1\n");
        }
    }

    footer_t* below = get_below_footer(block);
    if(contained_in_block((void*)below) && below->magic_number == MAGIC_NUMBER) {
        return coalesce(get_header(below));
    }
    
    /*
    // Check below
    footer_t* below = get_below_footer(block);
    //assert(below == get_footer(get_header(below)));
    //assert(get_above_header(get_header(below)) == block);
    // Check magic number
    if(contained_in_block((void*)below) && below->magic_number == MAGIC_NUMBER) {
        if(!is_allocated_footer(below)) {
            memory_block_t* below_head = get_header(below);
            remove_from_list(block);
            remove_from_list(below_head);
            size_t combined_size = get_size(block) + get_size(below_head) + HEADER_SIZE;
            put_block(below_head, combined_size, false);
            printf("Coalesced2\n");
            return below_head;
        }
    }
    */

    return block;
}

/*
 * get_padded_size - returns size with padding so that it will mainain alignment
 */

size_t get_padded_size(size_t size) {
    int remainder = size % ALIGNMENT;
    if(remainder == 0) {
        return size;
    } else {
        return size + (ALIGNMENT - (size % ALIGNMENT));
    }
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
    sbrk_block_size = get_padded_size(sizeof(sbrk_block));
    block_head = NULL;
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
            block = coalesce(block);
            insert(block);
            printf("list_size = %d\n", list_size);
            printf("Check_heap: %d", check_heap());
        }
    }
}
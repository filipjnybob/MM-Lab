#include "umalloc.h"
#include "csbrk.h"
#include "ansicolors.h"
#include <stdio.h>
#include <assert.h>

/*
 * For this memory allocator, free blocks are structured with a header and a footer.
 * The header contains the size of the block, its allocation status, pointers to the
 * next and previous free blocks, and a field for a magic number used to validate 
 * each block. The header appears at the start of the free block, while the footer
 * appears at the end after the data.
 * 
 * Allocated blocks use the same structure as free blocks, but they are marked as 
 * allocated and have both their previous and next pointers NULLed out.
 * 
 * The free list is organized in non-decreasing order of size in a doubly-linked list.
 * Blocks at the start of the list must be smaller or equal in size to blocks at the 
 * end.
 * 
 * Each time umalloc is called, the free list is searched for a block of best fit, or 
 * one of smallest size that is big enough for the query. If there is not a large 
 * enough block available, the heap is extended to make room. The found block is then
 * split into an allocated block in the lower addresses and a free block in the upper
 * addresses. The allocated block is of the minimum size needed to satisfy the request,
 * while the free block is all the leftover space. The free block is added to the free
 * list while the payload of the allocated block is returned to the user. All returned
 * pointers are 16-bit aligned.
 * 
 * Each time ufree is called, the validity of the given pointer is checked. If it is
 * a valid block, it is marked as unallocated, coalesced with neighboring free blocks,
 * and added to the free list.
 */

const char author[] = ANSI_BOLD ANSI_COLOR_RED "Isaac Adams EID: iga263" ANSI_RESET;

const int MAGIC_NUMBER = 0x12345678;

const int HEADER_SIZE = sizeof(memory_block_t) + sizeof(footer_t);

sbrk_block *block_head;

int heap_size = 0;

/*
 * The following helpers can be used to interact with the memory_block_t
 * struct, they can be adjusted as necessary.
 */

// A sample pointer to the start of the free list.
memory_block_t *free_head;

/*
 * is_allocated - Given a header, returns true if a block is marked as allocated.
 */
bool is_allocated(memory_block_t *block)
{
    assert(block != NULL);
    return block->block_size_alloc & 0x1;
}

/*
 * is_allocated_footer - Given a footer, returns true if a block is marked as allocated.
 */
bool is_allocated_footer(footer_t *block)
{
    assert(block != NULL);
    return block->block_size_alloc & 0x1;
}

/*
 * allocate - marks a block as allocated. Modified to also mark the footer
 */
void allocate(memory_block_t *block)
{
    assert(block != NULL);
    block->block_size_alloc |= 0x1;
    footer_t *footer = get_footer(block);
    footer->block_size_alloc = block->block_size_alloc;
}

/*
 * deallocate - marks a block as unallocated. Modified to also mark the footer.
 */
void deallocate(memory_block_t *block)
{
    assert(block != NULL);
    block->block_size_alloc &= ~0x1;
    footer_t *footer = get_footer(block);
    footer->block_size_alloc = block->block_size_alloc;
}

/*
 * get_size - gets the size of the block.
 */
size_t get_size(memory_block_t *block)
{
    assert(block != NULL);
    return block->block_size_alloc & ~(ALIGNMENT - 1);
}

/*
 * get_size_footer - Given a footer, returns the size of the block
 */
size_t get_size_footer(footer_t *footer)
{
    assert(footer != NULL);
    return footer->block_size_alloc & ~(ALIGNMENT - 1);
}

/*
 * get_next - gets the next block.
 */
memory_block_t *get_next(memory_block_t *block)
{
    assert(block != NULL);
    return block->next;
}

/*
 * put_block - puts a block struct into memory at the specified address.
 * Initializes the size, allocated, and magic number fields, along with NUlling out the next 
 * field. Modified to also place the footer in the appropriate memory location.
 */
void put_block(memory_block_t *block, size_t size, bool alloc)
{
    assert(block != NULL);
    assert(size % ALIGNMENT == 0);
    assert(alloc >> 1 == 0);
    // put heaer
    block->block_size_alloc = size | alloc;
    block->magic_number = MAGIC_NUMBER;
    block->prev = NULL;
    block->next = NULL;

    // put footer
    footer_t *footer = get_footer(block);
    footer->block_size_alloc = block->block_size_alloc;
    footer->magic_number = MAGIC_NUMBER;
}

/*
 * get_payload - gets the payload of the block.
 */
void *get_payload(memory_block_t *block)
{
    assert(block != NULL);
    return (void *)(block + 1);
}

/*
 * get_block - given a payload, returns the block.
 */
memory_block_t *get_block(void *payload)
{
    assert(payload != NULL);
    return ((memory_block_t *)payload) - 1;
}

/*
 * get_footer - given a header, gets the footer of the block.
 */
footer_t *get_footer(memory_block_t *block)
{
    assert(block != NULL);
    return ((void *)(block + 1)) + get_size(block);
}

/*
 * get_header - given a footer, gets the header of the block.
 */
memory_block_t *get_header(footer_t *footer)
{
    assert(footer != NULL);
    void *temp = ((void *)footer) - get_size_footer(footer);
    return ((memory_block_t *)temp) - 1;
}

/*
 * The following are helper functions that can be implemented to assist in your
 * design, but they are not required. 
 */

/*
 * insert - inserts the given free block into the free list in order of increasing size.
 */
void insert(memory_block_t *block)
{
    // Check if list is empty
    if (free_head == NULL)
    {
        // Insert block as only element
        free_head = block;
        free_head->next = free_head;
        free_head->prev = free_head;
    }
    else if (get_size(block) <= get_size(free_head))
    {
        // Insert block at start
        block->next = free_head;
        block->prev = free_head->prev;
        free_head->prev->next = block;
        free_head->prev = block;
        free_head = block;
    }
    else
    {
        // Search through free list for proper position
        memory_block_t *previous = free_head;
        memory_block_t *current = get_next(free_head);
        while (current != free_head && get_size(block) > get_size(current))
        {
            previous = current;
            current = get_next(current);
        }
        // Insert block at found position
        block->next = current;
        block->prev = previous;
        previous->next = block;
        current->prev = block;
    }
}

/*
 * insert_at_end - inserts given free block at the end of the free list
 */
void insert_at_end(memory_block_t *block)
{
    // Check if list is empty
    if (free_head == NULL)
    {
        // Insert as only element
        block->next = block;
        block->prev = block;
        free_head = block;
    }
    else
    {
        // Insert as last element
        block->next = free_head;
        block->prev = free_head->prev;
        free_head->prev->next = block;
        free_head->prev = block;
    }
}

/*
 * print_list - Prints out the addresses of each element of the list in order
 */
void print_list()
{
    memory_block_t *temp = free_head;
    do
    {
        printf("Address: %p, ", temp);
        temp = temp->next;
    } while (temp != free_head);
    printf("\n");
}

/*
 * remove_from_list - removes the given block from the free list if it is present
 */
void remove_from_list(memory_block_t *block)
{
    // Check that the block is still in the list
    if (block->next != NULL && block->prev != NULL)
    {
        if (block->next == block)
        {
            // block is the only element in the free list
            free_head = NULL;
            block->prev = NULL;
            block->next = NULL;
        }
        else
        {
            block->prev->next = block->next;
            block->next->prev = block->prev;

            if (free_head == block)
            {
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
memory_block_t *get_above_header(memory_block_t *block)
{
    return (memory_block_t *)(get_footer(block) + 1);
}

/*
 * get_below_footer - returns a pointer to the footer of the block below the given block
 */
footer_t *get_below_footer(memory_block_t *block)
{
    return ((footer_t *)block) - 1;
}

/*
 * find - finds a free block that can satisfy the umalloc request. Free block found with best fit.
 * Free list should be sorted in increasing size. Extends the heap if not enough space is available.
 */
memory_block_t *find(size_t size)
{

    // Check if free list is empty
    if (free_head == NULL)
    {
        return extend(size);
    }

    // Handle comparison with free_head
    if (get_size(free_head) >= size)
    {
        return free_head;
    }

    // Compare size to the size of each free block
    memory_block_t *current = get_next(free_head);
    while (current != free_head && get_size(current) < size)
    {
        current = get_next(current);
    }
    // Check if a big enough block was found
    if (current == free_head)
    {
        // No block found, extend the heap
        current = extend(size);
    }
    return current;
}

/*
 * put_sbrk_block - Initializes a sbrk_block at the given address with the given size
 * and puts it at the head of the block list.
 */
void put_sbrk_block(sbrk_block *block, size_t size)
{
    block->sbrk_start = (uint64_t)(((void *)block) + get_padded_size(sizeof(sbrk_block)));
    block->sbrk_end = (uint64_t)((void *)block->sbrk_start) + size;
    block->next = block_head;
    block_head = block;
}

/*
 * extend - extends the heap if more memory is required. Returns NULL if an error occurred.
 */
memory_block_t *extend(size_t size)
{
    int sbrk_block_size = get_padded_size(sizeof(sbrk_block));
    size_t extend_size = get_padded_size(heap_size * 2 + size);
    int max_size = ALIGNMENT * PAGESIZE - HEADER_SIZE - sbrk_block_size;
    // Ensure size is not too large to store
    if (size > max_size)
    {
        // size is greater than the largest possible block size
        return NULL;
    }
    // Ensure extension is not too large
    if (extend_size > max_size)
    {
        extend_size = max_size;
    }
    heap_size += extend_size;
    void *result = csbrk(get_block_size(extend_size) + sbrk_block_size);
    if (result == NULL)
    {
        return NULL;
    }

    // Create the sbrk block
    put_sbrk_block(result, get_block_size(extend_size));
    // Set result to the start of the sbrk block
    result = (void *)block_head->sbrk_start;
    put_block(result, extend_size, false);
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
memory_block_t *split(memory_block_t *block, size_t size)
{
    remove_from_list(block);
    size_t remaining_size = get_size(block) - get_padded_size(size);
    // Check that there's enough remaining size to split the block
    if (remaining_size < (HEADER_SIZE + ALIGNMENT))
    {
        // Not enough remaining space, don't split
        allocate(block);
        return block;
    }
    // Create the allocated block
    put_block(block, get_padded_size(size), true);
    // Create the free block
    memory_block_t *free = get_above_header(block);
    put_block(free, remaining_size - HEADER_SIZE, false);
    insert(free);

    // Return allocated block
    return block;
}

/*
 * contained_in_block - Returns true if the given pointer lies in one of
 * the allocated blocks given by csbrk.
 */
bool contained_in_block(void *ptr)
{
    sbrk_block *temp = block_head;
    uint64_t address = (uint64_t)ptr;
    // Loop through each sbrk block
    while (temp != NULL)
    {
        // Check if pointer lies in temp
        if (address >= temp->sbrk_start && address < temp->sbrk_end)
        {
            return true;
        }
        temp = temp->next;
    }

    return false;
}

/*
 * coalesce - coalesces a free memory block with neighbors.
 */
memory_block_t *coalesce(memory_block_t *block)
{

    // Check above
    memory_block_t *above = get_above_header(block);
    // Check magic number
    if (contained_in_block((void *)above) && above->magic_number == MAGIC_NUMBER)
    {
        if (!is_allocated(above))
        {
            // Found free neighbor. Create new combined block
            remove_from_list(block);
            remove_from_list(above);
            size_t combined_size = get_size(block) + get_size(above) + HEADER_SIZE;
            put_block(block, combined_size, false);
        }
    }

    // Check below
    footer_t *below = get_below_footer(block);
    // Check magic number
    if (contained_in_block((void *)below) && below->magic_number == MAGIC_NUMBER)
    {
        memory_block_t *below_head = get_header(below);
        if (!is_allocated(below_head))
        {
            // Found free neighbor. Create new combined block
            remove_from_list(block);
            remove_from_list(below_head);
            size_t combined_size = get_size(block) + get_size(below_head) + HEADER_SIZE;
            put_block(below_head, combined_size, false);
            block = below_head;
        }
    }

    return block;
}

/*
 * get_padded_size - returns size with padding so that it will mainain alignment
 */

size_t get_padded_size(size_t size)
{
    int remainder = size % ALIGNMENT;
    if (remainder == 0)
    {
        return size;
    }
    else
    {
        return size + (ALIGNMENT - (size % ALIGNMENT));
    }
}

/*
 * get_block_size - returns the total size needed to store a block with the given size
 */
size_t get_block_size(size_t size)
{
    return get_padded_size(size) + HEADER_SIZE;
}

/*
 * uinit - Used initialize metadata required to manage the heap
 * along with allocating initial memory.
 */
int uinit()
{
    block_head = NULL;
    size_t initial_size = (ALIGNMENT * 5);
    // Create initial heap
    memory_block_t *result = extend(initial_size);
    if (result == NULL)
    {
        return -1;
    }
    return 0;
}

/*
 * umalloc -  allocates size bytes and returns a pointer to the allocated memory.
 * Returns NULL if an error occurs.
 */
void *umalloc(size_t size)
{
    memory_block_t *block = find(size);
    if (block == NULL)
    {
        return block;
    }
    block = split(block, size);
    return get_payload(block);
}

/*
 * ufree -  frees the memory space pointed to by ptr, which must have been called
 * by a previous call to malloc.
 */
void ufree(void *ptr)
{
    memory_block_t *block = get_block(ptr);
    if (block != NULL)
    {
        // Check valid block
        if (contained_in_block(block) && block->magic_number == MAGIC_NUMBER)
        {
            // Valid block, free it
            deallocate(block);
            block = coalesce(block);
            insert(block);
        }
    }
}
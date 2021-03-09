
#include "umalloc.h"

//Place any variables needed here from umalloc.c as an extern.
extern memory_block_t *free_head;
extern const int MAGIC_NUMBER;

const int INVALID_BLOCK = 1;
const int ALLOCATED = 2;
const int INVALID_FOOTER = 3;
const int FOOTER_MISMATCH = 4;
const int CONTIGUOUS = 5;

/*
 * check_heap -  used to check that the heap is still in a consistent state.
 * Required to be completed for checkpoint 1.
 * Should return 0 if the heap is still consistent, otherwise return a non-zero
 * return code. Asserts are also a useful tool here.
 */
int check_heap() {
    memory_block_t* free_block = free_head;

    // Check each free block in the free list
    while(free_block != NULL) {
        
        // Check 1 - Is the pointer pointing to a valid block? Check the MAGIC_NUMBER
        if(free_block->magic_number != MAGIC_NUMBER) {
            return INVALID_BLOCK;
        }
        
        // Check 2 - Is the block free?
        if(is_allocated(free_block)) {
            return ALLOCATED;
        }

        // Check 3 - Does the information in the header and footer match?
        footer_t* footer = get_footer(free_block);

        // Check that footer is a valid footer
        if(footer->magic_number != MAGIC_NUMBER) {
            return INVALID_FOOTER;
        }

        // Compare the data between the header and the footer
        if(free_block->block_size_alloc != footer->block_size_alloc) {
            return FOOTER_MISMATCH;
        }

        // Check 4 - Are there any free blocks adjacent to this one?
        // Check below
        footer = ((footer_t*) free_block) - 1;
        if(footer->magic_number == MAGIC_NUMBER) {
            // Valid footer
            if(!is_allocated_footer(footer)) {
                // Found contiguous free block, not coalesced
                return CONTIGUOUS;
            }
        }

        // Check above
        memory_block_t* header = (memory_block_t*)(get_footer(free_block) + 1);
        if(header->magic_number == MAGIC_NUMBER) {
            // Valid header
            if(!is_allocated(header)) {
                // Found contiguous free block, not coalesced
                return CONTIGUOUS;
            }
        }

        free_block = free_block->next;
    }

    // Consistent Heap
    return 0;
}
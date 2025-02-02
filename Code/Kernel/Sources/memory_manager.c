#include "../Includes/memory_manager.h"

// Pointers for managing the heap's free list
static FreeBlock* current_free_block = NULL;
static void* alloc_list_start = NULL;
static void* alloc_list_end = NULL;

uint32_t pagesAllocated = 0;

void* page_alloc(uint32_t num_pages) {
    void* allocated_address = find_free_pages(num_pages);
    if (allocated_address == NULL) {
        printf("Warning: page_alloc failed to allocate memory! [Page Allocation Error Code: %d]\n", RED_ON_BLACK_WARNING);
        return NULL;
    }
    pagesAllocated += num_pages;
    return (void*)((uintptr_t)allocated_address + KERNEL_HIGH_BASE);
}

void free_large_pages(void* block, uint32_t size) {
    uintptr_t address = (uintptr_t)block - KERNEL_HIGH_BASE;
    uint32_t start_page = (address - MEM_ALLOC_START) / PAGE_SIZE;
    uint32_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t i = 0; i < num_pages; i++) {
        mark_page_as_free(start_page + i);
        pagesAllocated--;
    }
}

void init_free_list() {
    alloc_list_start = page_alloc(PAGES_FOR_HEAP);  // Allocate 4 pages for the heap (4096 * 4 bytes)
    if (alloc_list_start == NULL) {
        printf("Error: Could not allocate initial pages for the alloc list.\n", RED_ON_BLACK_WARNING);
        return;
    }

    alloc_list_end = (void*)((uintptr_t)alloc_list_start + (PAGE_SIZE * PAGES_FOR_HEAP));

    current_free_block = (FreeBlock*)alloc_list_start;
    current_free_block->size = PAGE_SIZE * PAGES_FOR_HEAP;
    current_free_block->address = (void*)((uintptr_t)alloc_list_start + sizeof(FreeBlock));
    current_free_block->next = NULL;
    current_free_block->isFree = true;
}

void* malloc(uint32_t size) {
    if (size == 0) return NULL;

    if (size > 4096) {
        uint32_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        void* new_address = page_alloc(num_pages);
        if (new_address == NULL) {
            printf("Error: Could not allocate new pages for malloc.\n", RED_ON_BLACK_WARNING);
            return NULL;
        }

        // Store the number of pages in the header at the start of the allocated memory
        AllocationHeader* header = (AllocationHeader*)new_address;
        header->num_pages = num_pages;

        // Return the memory address after the header
        return (void*)((uintptr_t)new_address + sizeof(AllocationHeader));
    } else {
        // Allocate from the free list for smaller sizes
        FreeBlock* temp = current_free_block;
        FreeBlock* prev = NULL;

        while (temp != NULL) {
            if (temp->isFree && temp->size >= size) {
                // Split the block if necessary
                if (temp->size > size + sizeof(FreeBlock)) {
                    FreeBlock* new_block = (FreeBlock*)((uintptr_t)temp + sizeof(FreeBlock) + size);
                    new_block->size = temp->size - size - sizeof(FreeBlock);
                    new_block->address = (void*)((uintptr_t)new_block + sizeof(FreeBlock));
                    new_block->isFree = true;
                    new_block->next = temp->next;

                    temp->size = size;
                    temp->next = new_block;
                }

                temp->isFree = false;
                return temp->address;
            }
            prev = temp;
            temp = temp->next;
        }

        // If no suitable block was found
        printf("Error: No suitable block found in the heap for malloc.\n", RED_ON_BLACK_WARNING);
        return NULL;
    }
}

void free(void* block) {
    if (block == NULL) return;

    uintptr_t block_address = (uintptr_t)block;

    // Check if the block is part of the heap (less than 4096 bytes)
    if (block_address >= (uintptr_t)alloc_list_start && block_address < (uintptr_t)alloc_list_end) {
        FreeBlock* temp = current_free_block;
        while (temp != NULL) {
            if (temp->address == block) {
                temp->isFree = true;
                // Attempt to merge with adjacent free blocks
                if (temp->next != NULL && temp->next->isFree) {
                    temp->size += temp->next->size + sizeof(FreeBlock);
                    temp->next = temp->next->next;
                }
                break;
            }
            temp = temp->next;
        }
    } else {
        // Handle larger allocations by reading the metadata header
        AllocationHeader* header = (AllocationHeader*)((uintptr_t)block - sizeof(AllocationHeader));
        uint32_t num_pages = header->num_pages;

        // Calculate the starting physical address to free
        uintptr_t start_address = (uintptr_t)header - KERNEL_HIGH_BASE - MEM_ALLOC_START;

        // Free the pages in the bitmap
        pagesAllocated -= num_pages;

        for (uint32_t i = 0; i < num_pages; i++) {
            mark_page_as_free((start_address / PAGE_SIZE) + i);
        }
    }
}

void* calloc(uint32_t num, uint32_t size) {
    void* ptr = malloc(num * size);
    if (ptr != NULL) memset(ptr, 0, num * size);
    return ptr;
}

void* realloc(void* ptr, uint32_t new_size) {
    if (ptr == NULL) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }
    
    void* new_ptr = malloc(new_size);
    if (new_ptr == NULL) return NULL;

    memcpy(new_ptr, ptr, new_size);
    free(ptr);

    return new_ptr;
}

void* realloc_safe(void* ptr, uint32_t new_size, uint32_t ptr_size) {
    if (ptr == NULL) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* new_ptr = malloc(new_size);
    if (new_ptr == NULL) return NULL;

    uint32_t copy_size = (ptr_size < new_size) ? ptr_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    
    free(ptr);
    return new_ptr;
}
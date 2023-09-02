#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX 1000000
#define PAGE_SIZE 4096
#define NUM_PAGES 16*1024*1024/PAGE_SIZE
#define NUM_FRAMES 1024*1024/PAGE_SIZE
//These are preprocessor macros. MAX is set to 1,000,000, PAGE_SIZE is set to 4096 bytes 
//(4KB, a common page size in many systems), NUM_PAGES is the total number of pages in the virtual memory 
//(16MB divided by page size), and NUM_FRAMES is the total number of frames in the physical memory (1MB divided by page size).

typedef struct {
    int valid;
    int unswappable;
    int read_bit;
    int write_bit;
    int frame;
    int last_used_time;
    int second_chance;
} PageTableEntry;

//This defines a PageTableEntry struct, which represents an entry in the page table. 
//Each entry contains several fields: valid (whether the page is in memory), 
//unswappable (whether the page can be swapped out), read_bit and write_bit (used for tracking page access), 
//frame (the frame number where the page is loaded in physical memory), last_used_time (the last time the page was accessed), 
//and second_chance (used for the second chance page replacement algorithm).

typedef struct {
    PageTableEntry* pageTable;
    char* physicalMemory;
    FILE* swapFile;
    int time;
    int fifo_ptr;
} MMU;
//This defines an MMU struct, which represents the Memory Management Unit. 
//It contains a pointer to the page table, a pointer to the physical memory, 
//a file pointer to the swap file, a time counter, and a pointer for the FIFO (First In, First Out) page replacement strategy.

MMU* MMU_init();

void MMU_exception(MMU* mmu, int pos);

void MMU_writeByte(MMU* mmu, int pos, char c);

char MMU_readByte(MMU* mmu, int pos);


MMU* MMU_init() {
    MMU* mmu = malloc(sizeof(MMU));
    mmu->pageTable = malloc(NUM_PAGES * sizeof(PageTableEntry));
    mmu->physicalMemory = malloc(NUM_FRAMES * PAGE_SIZE);
    for (int i = 0; i < NUM_PAGES; i++) {
        mmu->pageTable[i].valid = (i < NUM_FRAMES) ? 1 : 0;  // Initialize the first NUM_FRAMES pages to be valid
        mmu->pageTable[i].unswappable = 0;
        mmu->pageTable[i].read_bit = 0;
        mmu->pageTable[i].write_bit = 0;
        mmu->pageTable[i].frame = (i < NUM_FRAMES) ? i : -1;  // Initialize the first NUM_FRAMES pages to have a frame
        mmu->pageTable[i].last_used_time = -1;
        mmu->pageTable[i].second_chance = 0;
    }
    mmu->swapFile = fopen("swapFile", "w+");
    mmu->time = 0;
    mmu->fifo_ptr = 0;
    return mmu;
}


// This function writes a byte to a specific position in memory
void MMU_writeByte(MMU* mmu, int pos, char c) {
    // Calculate the page number and offset within the page based on the given position
    int page = pos / PAGE_SIZE;
    int offset = pos % PAGE_SIZE;

    // If the page is not marked as valid in the page table, call the exception handling function
    if (!mmu->pageTable[page].valid) 
        MMU_exception(mmu, pos);

    // If the page is marked as valid in the page table
    if (mmu->pageTable[page].valid) {
        // If there's a frame associated with this page
        if(mmu->pageTable[page].frame != -1) {
            // Set the write bit to 1 and write the character to the corresponding position in physical memory
            mmu->pageTable[page].write_bit = 1;
            mmu->physicalMemory[mmu->pageTable[page].frame * PAGE_SIZE + offset] = c;
        } else {
            // If there's no frame associated with this page, call the exception handling function
            MMU_exception(mmu, pos);
        }
    }
    // Increment the time counter
    mmu->time++;
}

// This function reads a byte from a specific position in memory
char MMU_readByte(MMU* mmu, int pos) {
    // Calculate the page number and offset within the page based on the given position
    int page = pos / PAGE_SIZE;
    int offset = pos % PAGE_SIZE;

    // If the page is not marked as valid in the page table, call the exception handling function
    if (!mmu->pageTable[page].valid) 
        MMU_exception(mmu, pos);
    
    // If the page is marked as valid in the page table
    if (mmu->pageTable[page].valid) {
        // If there's a frame associated with this page
        if(mmu->pageTable[page].frame != -1) {
            // Set the read bit to 1 and return the character from the corresponding position in physical memory
            mmu->pageTable[page].read_bit = 1;
            return mmu->physicalMemory[mmu->pageTable[page].frame * PAGE_SIZE + offset];
        } else {
            // If there's no frame associated with this page, call the exception handling function
            MMU_exception(mmu, pos);
        }
    }
}

// This function handles page faults
void MMU_exception(MMU* mmu, int pos) {
    // Calculate the page number based on the given position
    int page = pos / PAGE_SIZE;
    // Save the starting pointer for the FIFO replacement policy
    int start_ptr = mmu->fifo_ptr;
    printf("Exception at position %d (page %d)\n", pos, page);
    int frame = -1;
    // Keep looking for a frame to replace until one is found
    while (frame == -1) {
        // If the current page is either not valid or doesn't have a second chance, it can be replaced
        if (!mmu->pageTable[mmu->fifo_ptr].valid || mmu->pageTable[mmu->fifo_ptr].second_chance == 0) {
            // Write the content of the frame being replaced to the swap file
            printf("Replacing frame %d\n", mmu->fifo_ptr);
            fseek(mmu->swapFile, mmu->fifo_ptr * PAGE_SIZE, SEEK_SET);
            fwrite(&mmu->physicalMemory[mmu->fifo_ptr * PAGE_SIZE], 1, PAGE_SIZE, mmu->swapFile);
            
            // Mark the frame as the one to be used for the new page
            frame = mmu->fifo_ptr;
            // Invalidate the current page in the page table and unset its frame
            mmu->pageTable[mmu->fifo_ptr].valid = 0;
            mmu->pageTable[mmu->fifo_ptr].frame = -1;
            printf("Replaced frame %d with new frame %d\n", mmu->fifo_ptr, frame);
        } else if (mmu->pageTable[mmu->fifo_ptr].valid) {
            // If the current page is valid and has a second chance, remove its second chance
            mmu->pageTable[mmu->fifo_ptr].second_chance = 0;
        }

        // Move the FIFO pointer to the next frame
        mmu->fifo_ptr = (mmu->fifo_ptr + 1) % NUM_FRAMES;

        // If we've gone through all the frames without finding one to replace, stop the loop
        if (mmu->fifo_ptr == start_ptr){
            printf("Completed one loop through frames\n");
            break;
        }
    }

    // If a frame was found to replace
    if (frame != -1) {
        // Clear the memory in the new frame
        memset(&mmu->physicalMemory[frame * PAGE_SIZE], 0, PAGE_SIZE);

        // Read the content of the new page from the swap file into the new frame
        fseek(mmu->swapFile, page * PAGE_SIZE, SEEK_SET);
        fread(&mmu->physicalMemory[frame * PAGE_SIZE], 1, PAGE_SIZE, mmu->swapFile);

        // Mark the new page as valid in the page table, set its frame, update its last used time, and give it a second chance
        mmu->pageTable[page].valid = 1;
        mmu->pageTable[page].frame = frame;
        mmu->pageTable[page].last_used_time = mmu->time;
        mmu->pageTable[page].second_chance = 1;
        printf("Updated page table for page %d:\n", page);
        printf("  valid: %d\n", mmu->pageTable[page].valid);
        printf("  frame: %d\n", mmu->pageTable[page].frame);
        printf("  last_used_time: %d\n", mmu->pageTable[page].last_used_time);
        printf("  second_chance: %d\n", mmu->pageTable[page].second_chance);
    }
    printf("Loaded page %d into frame %d\n", page, frame);
}

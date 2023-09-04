
#include "mmu.h"

#include <stdio.h>
#include <stdlib.h>

MMU *MMU_initialize()
{
    MMU *mmu = (MMU *)malloc(sizeof(MMU));
    if (mmu == NULL)
    {
        perror("Failed to allocate MMU");
        exit(EXIT_FAILURE);
    }

    mmu->physical_memory = (char *)malloc(PHYSICAL_MEMORY_SIZE);
    if (mmu->physical_memory == NULL)
    {
        perror("Failed to allocate physical memory");
        exit(EXIT_FAILURE);
    }

    // Initialize page table entries
    for (int i = 0; i < NUM_PAGES; ++i)
    {
        mmu->page_table[i].valid = 0;
        mmu->page_table[i].unswappable = (rand() % 10 == 0);
        mmu->page_table[i].read_bit = 0;
        mmu->page_table[i].write_bit = 0;
        mmu->page_table[i].frame_number = -1;
    }

    // Open swap file
    mmu->swap_file = fopen("swap_file.bin", "wb+");
    if (mmu->swap_file == NULL)
    {
        perror("Failed to open swap file");
        exit(EXIT_FAILURE);
    }

    return mmu;
}

void MMU_writeByte(MMU *mmu, int pos, char c)
{
    int page_number = pos / PAGE_SIZE;
    int offset = pos % PAGE_SIZE;

    // Check if the page is valid
    if (!mmu->page_table[page_number].valid)
    {
        printf("Page fault at position: %d\n", pos);
        MMU_exception(mmu, pos);
    }
    else
    {
        printf("Page hit at position: %d\n", pos);
        printf("Page number: %d\n", page_number);
    }

    // Get frame number and write byte
    int frame_number = mmu->page_table[page_number].frame_number;
    int physical_pos = frame_number * PAGE_SIZE + offset;
    mmu->physical_memory[physical_pos] = c;

    // Update write_bit
    mmu->page_table[page_number].write_bit = 1;
}

char MMU_readByte(MMU *mmu, int pos)
{
    int page_number = pos / PAGE_SIZE;
    int offset = pos % PAGE_SIZE;

    // Check if the page is valid
    if (!mmu->page_table[page_number].valid)
    {
        MMU_exception(mmu, pos);
    }

    // Get frame number and read byte
    int frame_number = mmu->page_table[page_number].frame_number;
    int physical_pos = frame_number * PAGE_SIZE + offset;
    char value = mmu->physical_memory[physical_pos];

    // Update read_bit
    mmu->page_table[page_number].read_bit = 1;

    return value;
}

void MMU_exception(MMU *mmu, int pos)
{
    printf("MMU_exception called for position: %d\n", pos);

    int page_number = pos / PAGE_SIZE;
    int empty_frame = -1;

    // Check for empty space in physical memory
    for (int i = 0; i < NUM_FRAMES; ++i)
    {
        int is_frame_empty = 1;
        // Check if a page is mapped to this frame
        for (int j = 0; j < NUM_PAGES; ++j)
        {
            if (mmu->page_table[j].frame_number == i)
            {
                is_frame_empty = 0;
                break;
            }
        }
        if (is_frame_empty)
        {
            printf("Found empty frame: %d\n\n", i);
            empty_frame = i;
            break;
        }
    }

    // If no empty frame, use Second Chance algorithm
    if (empty_frame == -1)
    {
        printf("Applying Second Chance algorithm.\n");
        int found = 0;
        int cycles = 0; // Count how many cycles we go through
        
        while (!found && cycles < 4)
        {
            cycles++;
            for (int rw_combo = 0; rw_combo < 4; ++rw_combo)
            {
                int target_read_bit = (rw_combo & 2) >> 1;
                int target_write_bit = rw_combo & 1;
                
                int oldest_page = -1;
                for (int j = 0; j < NUM_PAGES; ++j)
                {
                    if (mmu->page_table[j].frame_number == mmu->oldest_frame_index)
                    {
                        oldest_page = j;
                        break;
                    }
                }
                
                // Skip unswappable pages
                if (mmu->page_table[oldest_page].unswappable)
                {
                    mmu->oldest_frame_index = (mmu->oldest_frame_index + 1) % NUM_FRAMES;
                    printf("Skipped unswappable page: %d\n", oldest_page);
                    continue;
                }
                
                if (mmu->page_table[oldest_page].read_bit == target_read_bit &&
                    mmu->page_table[oldest_page].write_bit == target_write_bit)
                {
                    // Replace this page
                    printf("Found page to replace: %d\n", oldest_page);
                    empty_frame = mmu->oldest_frame_index;
                    found = 1;
                    break;
                }
                else
                {
                    // Give a second chance and set the read and write bits to the lowest possible values
                    mmu->page_table[oldest_page].read_bit = 0;
                    mmu->page_table[oldest_page].write_bit = 0;
                }
                
                // Move the pointer to the next frame
                mmu->oldest_frame_index = (mmu->oldest_frame_index + 1) % NUM_FRAMES;
            }
        }
        
        if (cycles >= 4)
        {
            printf("Could not find a page to replace after 4 cycles. Taking additional measures.\n");
            // Handle this scenario appropriately
        }
        
        printf("Iterations taken: %d\n", cycles * 4);
    }

    // Update page table and swap in/out as necessary
    //  If replacing an old page, write it back to the swap file
    if (empty_frame != -1)
    {
        for (int j = 0; j < NUM_PAGES; ++j)
        {
            if (mmu->page_table[j].frame_number == empty_frame)
            {
                fseek(mmu->swap_file, j * PAGE_SIZE, SEEK_SET);
                fwrite(&mmu->physical_memory[empty_frame * PAGE_SIZE], 1, PAGE_SIZE, mmu->swap_file);
                mmu->page_table[j].valid = 0; // Mark old page as invalid
                break;
            }
        }
    }

    // Read the new page into the frame
    fseek(mmu->swap_file, page_number * PAGE_SIZE, SEEK_SET);
    fread(&mmu->physical_memory[empty_frame * PAGE_SIZE], 1, PAGE_SIZE, mmu->swap_file);

    // Update the page table
    mmu->page_table[page_number].valid = 1;
    mmu->page_table[page_number].frame_number = empty_frame;

    // Reset read and write bits
    mmu->page_table[page_number].read_bit = 0;
    mmu->page_table[page_number].write_bit = 0;
}

void MMU_cleanup(MMU *mmu)
{
    // Close the swap file
    fclose(mmu->swap_file);

    // Free the allocated physical memory
    free(mmu->physical_memory);

    // Free the MMU structure itself
    free(mmu);
}

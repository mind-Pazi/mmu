
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

    mmu->free_frames_top = 0;
    for (int i = 0; i < NUM_FRAMES; ++i)
    {
        mmu->free_frames[i] = i;
        mmu->free_frames_top++;
    }
    return mmu;
}

void MMU_writeByte(MMU *mmu, int pos, char c)
{
    int page_number = pos / PAGE_SIZE;
    int offset = pos % PAGE_SIZE;

    printf("Writing byte %d at position: %d\n", c, pos);
    
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

    printf("Reading byte at position: %d\n", pos);

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
    if (mmu->free_frames_top > 0)
    {
        empty_frame = mmu->free_frames[--mmu->free_frames_top];
        printf("Found empty frame from free list: %d\n\n", empty_frame);
    }

    // If no empty frame, use Second Chance algorithm
    if (empty_frame == -1)
    {
        printf("Applying Second Chance algorithm.\n");
        int found = 0;
        int iterations = 0;
        while (!found)
        {
            iterations++;
            int oldest_page = -1;
            for (int j = 0; j < NUM_PAGES; ++j)
            {
                if (mmu->page_table[j].frame_number == mmu->oldest_frame_index)
                {
                    oldest_page = j;
                    break;
                }
            }

            // Check read and write bits for each condition: 00, 10, 01, 11
            for (int condition = 0; condition <= 3; ++condition)
            {
                int read_bit = (condition & 2) >> 1;
                int write_bit = condition & 1;

                // Skip unswappable pages or pages not matching the condition
                if (mmu->page_table[oldest_page].unswappable ||
                    mmu->page_table[oldest_page].read_bit != read_bit ||
                    mmu->page_table[oldest_page].write_bit != write_bit)
                {
                    mmu->oldest_frame_index = (mmu->oldest_frame_index + 1) % NUM_FRAMES;
                    continue;
                }

                // Replace this page
                printf("Found page to replace: %d\n", oldest_page);
                empty_frame = mmu->oldest_frame_index;
                found = 1;
                break;
            }

            if (found)
                break;

            // Give a second chance to all pages
            for (int j = 0; j < NUM_PAGES; ++j)
            {
                mmu->page_table[j].read_bit = 0;
                mmu->page_table[j].write_bit = 0;
            }
        }

        printf("Iterations taken: %d\n\n", iterations);
    }

    // Update page table and swap in/out as necessary
    //  If replacing an old page, write it back to the swap file
    if (empty_frame != -1)
    {
        for (int j = 0; j < NUM_PAGES; ++j)
        {
            if (mmu->page_table[j].frame_number == empty_frame)
            {
                // Only write back to disk if the page has been modified
                if (mmu->page_table[j].write_bit)
                {
                    fseek(mmu->swap_file, j * PAGE_SIZE, SEEK_SET);
                    fwrite(&mmu->physical_memory[empty_frame * PAGE_SIZE], 1, PAGE_SIZE, mmu->swap_file);
                }
                mmu->page_table[j].valid = 0; // Mark old page as invalid
                mmu->free_frames[mmu->free_frames_top++] = empty_frame;
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

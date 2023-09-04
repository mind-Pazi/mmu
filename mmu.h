#include <stdio.h>
#include <stdlib.h>

// Define constants
#define VIRTUAL_MEMORY_SIZE 16777216 // 16 MB
#define PHYSICAL_MEMORY_SIZE 1048576 // 1 MB
#define PAGE_SIZE 4096               // 4 KB
#define NUM_PAGES (VIRTUAL_MEMORY_SIZE / PAGE_SIZE)
#define NUM_FRAMES (PHYSICAL_MEMORY_SIZE / PAGE_SIZE)
// Page table entry structure
typedef struct
{
    int valid;
    int unswappable;
    int read_bit;
    int write_bit;
    int frame_number;
} PageTableEntry;

// MMU structure
typedef struct
{
    PageTableEntry page_table[NUM_PAGES];
    char *physical_memory;
    FILE *swap_file;
    int oldest_frame_index;
} MMU;

// Function declarations
MMU *MMU_initialize();
void MMU_writeByte(MMU *mmu, int pos, char c);
char MMU_readByte(MMU *mmu, int pos);
void MMU_exception(MMU *mmu, int pos);
void MMU_cleanup(MMU *mmu);
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

    // Step 1: Check for empty space in physical memory
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

    // Step 2: If no empty frame, use Second Chance algorithm
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

            // Skip unswappable pages
            if (mmu->page_table[oldest_page].unswappable)
            {
                mmu->oldest_frame_index = (mmu->oldest_frame_index + 1) % NUM_FRAMES;
                printf("Skipped unswappable page: %d\n", oldest_page);
                continue;
            }

            if (mmu->page_table[oldest_page].read_bit == 0 && mmu->page_table[oldest_page].write_bit == 0)
            {
                // Replace this page
                printf("Found page to replace: %d\n", oldest_page);
                empty_frame = mmu->oldest_frame_index;
                found = 1;
            }
            else
            {
                // Give a second chance
                mmu->page_table[oldest_page].read_bit = 0;
                mmu->page_table[oldest_page].write_bit = 0;
            }

            // Move the pointer to the next frame
            mmu->oldest_frame_index = (mmu->oldest_frame_index + 1) % NUM_FRAMES;
        }
        printf("Iterations taken: %d\n", iterations);
    }

    // Step 3: Update page table and swap in/out as necessary
    // If replacing an old page, write it back to the swap file
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

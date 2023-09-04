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

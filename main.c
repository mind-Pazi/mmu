#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mmu.h"

// Maximum number of iterations
#define MAX 1000000

// The main function
int main(int argc, char** argv) {
    // Initialize the Memory Management Unit (MMU)
    MMU* mmu = MMU_init();

    // Seed the random number generator with the current time
    srand(time(NULL));

    // Run a loop MAX times
    for(int i = 0; i < 10000; i++) {
        // Generate a random position in the memory
        int pos = rand() % (NUM_PAGES * PAGE_SIZE);

        // Generate a random character
        char c = rand() % 256;
        printf("Writing %d to position %d\n", c, pos);
        // Write the random character to the random position in memory
        MMU_writeByte(mmu, pos, c);

        // Read the character from the same position in memory
        char c2 = MMU_readByte(mmu, pos);
        printf("Reading from position %d: %d\n", pos, c2);
        // If the written and read characters do not match, print an error message and exit the program
        if(c != c2) {
            printf("Error at pos %d\n", pos);
            exit(-1);
        }
    }

    // If the loop finished without errors, print a success message
    printf("Test passed\n");

    // Close the swap file
    fclose(mmu->swapFile);

    // Free the physical memory array
    free(mmu->physicalMemory);

    // Free the page table
    free(mmu->pageTable);

    // Free the MMU object itself
    free(mmu);

    // Return 0 to indicate successful execution
    return 0;
}

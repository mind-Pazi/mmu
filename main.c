#include <stdio.h>
#include "mmu.h"
#include <stdlib.h>
#include <time.h>

int main()
{
    // Initialize the MMU
    MMU *mmu = MMU_initialize();

    // Seed the random number generator
    srand(time(NULL));
    char c;
    char c2;
    int pos;
    // 1. Sequential Access
    printf("Sequential Access\n");
    for (int i = 0; i < 1000; i++)
    {
        c = rand() % 256;
        MMU_writeByte(mmu, i, c);
        c2 = MMU_readByte(mmu, i);
        if (c != c2)
        {
            printf("Error at pos %d\n", i);
            exit(-1);
        }
    }

    // 2. Random Access
    printf("Random Access\n");
    for (int i = 0; i < 1000; i++)
    {
        c = rand() % 256;
        pos = rand() % VIRTUAL_MEMORY_SIZE;
        MMU_writeByte(mmu, pos, c);
        c2 = MMU_readByte(mmu, pos);
        if (c != c2)
        {
            printf("Error at pos %d\n", i);
            exit(-1);
        }
    }
    // 3. Looping Access
    printf("Looping Access\n");
    for (int i = 0; i < 1000; i++)
    {
        c = rand() % 256;
        pos = i % 500; // small range to trigger page faults
        MMU_writeByte(mmu, pos, c);
        c2 = MMU_readByte(mmu, pos);
        if (c != c2)
        {
            printf("Error at pos %d\n", i);
            exit(-1);
        }
    }
    printf("\n");
    // 4. Boundary Conditions
    MMU_writeByte(mmu, 0, 'X');
    MMU_readByte(mmu, 0);
    MMU_writeByte(mmu, VIRTUAL_MEMORY_SIZE - 1, 'X');
    MMU_readByte(mmu, VIRTUAL_MEMORY_SIZE - 1);

    // Cleanup
    MMU_cleanup(mmu);
    printf("Test passed\n");
    return 0;
}

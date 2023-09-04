# Compiler and Compile options.
CC = gcc
CCOPTS = --std=gnu99 -Wall
AR = ar

# Object files and headers
OBJS = main.o mmu.o
HEADERS = mmu.h

# Library and Binary files
BINS = my_program

# Phony targets
.PHONY: clean all

# Default target
all: $(LIBS) $(BINS)

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@ $<

# Create binary
my_program: $(OBJS)
	$(CC) $(CCOPTS) -o $@ $^

# Clean
clean:
	rm -rf *.o *~ $(LIBS) $(BINS)

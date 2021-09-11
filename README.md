# Dynamic-Memory-Allocater
A system written in C for performing dynamic memory allocation, including allocating, freeing, and reallocating memory. <br>
This allocator applies the concept of segregated free lists as an efficient way to keep track of free blocks on the heap for allocation. <br>
The system stores information in headers and footers of free/allocated blocks, including the sizes of the blocks and whether the current and previous blocks in physical memory are allocated, to speed up the process of free block coalescing. 

## Features
This dynamic memory allocator can:
- Allocate memory
- Free memory
- Reallocate memory into smaller or bigger memory blocks
- Align memory blocks to a specific bit alignment

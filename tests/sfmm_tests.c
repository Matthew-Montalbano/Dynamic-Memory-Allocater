#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & ~0xf)) {
		cnt++;
	    }
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz = 4;
	int *x = sf_malloc(sz);

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(8112, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + 8192 == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	// We want to allocate up to exactly four pages, so there has to be space
	// for the header, footer, and the link pointers.
	void *x = sf_malloc(32704);
	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(524288);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(131024, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(208, 1);
	assert_free_block_count(7872, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 2);
	assert_free_block_count(528, 1);
	assert_free_block_count(7552, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
        size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200, sz_z = 700;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 4);
	assert_free_block_count(208, 3);
	assert_free_block_count(5968, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 8,
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 8);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert(bp->header & 0x1, "Allocated bit is not set!");
	cr_assert((bp->header & ~0xf) == 96,
		  "Realloc'ed block size (0x%ld) not what was expected (0x%ld)!",
		  bp->header & ~0xf, 96);

	assert_free_block_count(0, 2);
	assert_free_block_count(32, 1);
	assert_free_block_count(7984, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
        size_t sz_x = 80, sz_y = 64;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert(bp->header & 0x1, "Allocated bit is not set!");
	cr_assert((bp->header & ~0xf) == 96, "Block size not what was expected!");

	// There should be only one free block.
	assert_free_block_count(0, 1);
	assert_free_block_count(8048, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = 64, sz_y = 8;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert(bp->header & 0x1, "Allocated bit is not set!");
	cr_assert((bp->header & ~0xf) == 32, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size 48
	// to the freelist.  This block will go into the main freelist and be coalesced.
	assert_free_block_count(0, 1);
	assert_free_block_count(8112, 1);
}
//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

Test(sfmm_basecode_suite, realloc_zero_size, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(64);
	void *y = sf_realloc(x, 0);
	cr_assert_null(y, "y is not NULL!");

	assert_free_block_count(0, 1);
	assert_free_block_count(8144, 1); // 8144 = size of free block after heap initialization

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, memalign_address_already_aligned, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t align = 64;
	void *start_address = sf_mem_start() + 8 + 32 + 8; // mem_start + padding + prologue + first block header
	void *aligned_address = start_address;
	// Use while loop to find first address aligned to 64 with enough space to allocate a block before
	while (((uintptr_t) aligned_address % align != 0) || (aligned_address - start_address < 32)) {
		aligned_address += 8;
	}
	size_t malloc_size = aligned_address - start_address - 8; // aligned_address - start_address would give size of whole block, want only size of payload
	void *x = sf_malloc(malloc_size); // allocate a block so that the next payload address will be aligned to 64
	void *y = sf_memalign(8, align);
	cr_assert_not_null(y, "y is NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(8144 - (malloc_size + 8 + 32), 1); // 8144 = size of free block after heap initalization, (malloc_size + 8 + 32) = size of all allocated blocks
	cr_assert(x + malloc_size + 8 == y, "Memalign allocation not at the right address");
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, malloc_expand_heap_no_wilderness_block, .timeout = TEST_TIMEOUT) {
	void *x = sf_malloc(8136); // Fill heap without expanding so there is no wilderness block after allocation
	assert_free_block_count(0, 0);
	cr_assert_not_null(x, "x is NULL!");

	void *mem_end = sf_mem_end();
	void *y = sf_malloc(8);
	assert_free_block_count(0, 1);
	assert_free_block_count(PAGE_SZ - 32, 1);

	cr_assert(y == mem_end, "New allocation not at the right address");
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, realloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(64);
	void *y = sf_realloc(x, 10000000000000000);

	cr_assert_null(y, "y is not NULL!");

	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert(bp->header & 0x1, "Allocated bit is not set!");
	cr_assert((bp->header & ~0xf) == 80, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 1);
	assert_free_block_count(130944, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, realloc_invalid_pointer, .timeout = TEST_TIMEOUT) {
	void *x = sf_malloc(8);
	void *y = sf_realloc(x - 8, 1500);

	assert_free_block_count(0, 1);
	assert_free_block_count(8112, 1);
	cr_assert_null(y, "y is not NULL!");
	cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}
/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "my_sfmm.h"

void *sf_malloc(size_t size) {
	if (size == 0) {
		return NULL;
	}
	else if (sf_mem_start() == sf_mem_end()) { /* First time sf_malloc being called */
		initialize_free_lists();
		initialize_heap();
	}
	size_t block_size = calculate_aligned_block_size(size);
	sf_block *free_block = find_free_block(block_size);
	if (free_block == NULL) {
		free_block = expand_heap_to_fit(block_size);
		if (free_block == NULL) {
			return NULL;
		}
	}
	size_t free_block_size = free_block->header & ~(0xF);
	size_t split_size = free_block_size - block_size;
	if (split_size >= 32) {
		sf_block *split_block_addr = ((void *) free_block) + block_size;
		create_free_block(split_size, 1, split_block_addr);
	} else {
		block_size = free_block_size;
	}
	allocate_block(free_block, block_size);
    return ((void *) free_block + 8);
}




void initialize_heap() {
	sf_mem_grow();
	allocate_prologue();
	allocate_epilogue();
	int allocated_bytes = 8 + 32 + 8; /* padding + prologue + epilogue */
	size_t free_block_size = PAGE_SZ - allocated_bytes;
	sf_block *free_block_address = sf_mem_start() + 8 + 32; /* (8 + 32) = padding bytes + prologue bytes */
	create_free_block(free_block_size, 1, free_block_address);
}

void allocate_prologue() {
	sf_block *prologue_address = sf_mem_start() + 8; /* 8 bytes of padding */
	sf_header prologue = create_header(32, 0, 1);
	prologue_address->header = prologue;
}

void allocate_epilogue() {
	sf_block *epilogue_address = sf_mem_end() - 8; /* Epilogue starts 8 bytes from end of heap */
	sf_header epilogue = create_header(0, 0, 0);
	epilogue_address->header = epilogue;
}

void create_free_block(size_t block_size, int prv_alloc, sf_block *block_address) {
	sf_header header = create_header(block_size, prv_alloc, 0);
	block_address->header = header;
	sf_block *list_head = get_relevant_free_list_head(block_size, block_address);
	insert_into_free_list(block_address, list_head);
	sf_footer *footer = ((void *) block_address) + block_size - 8;
	*footer = header;
}

sf_block *get_relevant_free_list_head(size_t size, void *block) {
	if (block + size == sf_mem_end() - 8) { /* If block is wilderness block */
		return &sf_free_list_heads[7];
	}
	int min_block_size = 32;
	if (size > (32 * min_block_size)) {
		return &sf_free_list_heads[6];
	}
	for (int i = 0; i < NUM_FREE_LISTS - 1; i++) {
		if (power(2, i-1) * min_block_size < size && size <= power(2, i) * min_block_size) {
			return &sf_free_list_heads[i];
		}
	}
	return NULL;
}

void insert_into_free_list(sf_block *block, sf_block *list_head) {
	sf_block *next = list_head->body.links.next;
	block->body.links.prev = list_head;
	block->body.links.next = next;
	list_head->body.links.next = block;
	next->body.links.prev = block;
}

sf_header create_header(size_t block_size, int prv_alloc, int alloc) {
	sf_header header = block_size;
	if (prv_alloc == 1) {
		header |= PREV_BLOCK_ALLOCATED;
	}
	if (alloc == 1) {
		header |= THIS_BLOCK_ALLOCATED;
	}
	return header;
}



void initialize_free_lists() {
	sf_block *sentinel;
	for (int i = 0; i < NUM_FREE_LISTS; i++) {
		sentinel = &(sf_free_list_heads[i]);
		sentinel->body.links.prev = sentinel;
		sentinel->body.links.next = sentinel;
	}
}




size_t calculate_aligned_block_size(size_t size) {
	size_t block_size = size + 8; /* 8 bytes for header */
	if (block_size < 32) { /* Must be at least 32 bytes */
		block_size = 32;
	}
	if (block_size % 16 != 0) {
		block_size += 16 - (block_size % 16); /* Add padding */
	}
	return block_size;
}




sf_block *find_free_block(size_t size) {
	sf_block *block;
	for (int i = 0; i < NUM_FREE_LISTS; i++) {
		block = search_free_list(&sf_free_list_heads[i], size);
		if (block != NULL) {
			return block;
		}
	}
	return NULL;
}

sf_block *search_free_list(sf_block *head, size_t size) {
	if ((head->body.links.next == 0 && head->body.links.prev == 0) || (head->body.links.next == head && head->body.links.prev == head)) {
		return NULL;
	}
	sf_block *curr_block = head->body.links.next;
	while (curr_block != head) {
		if (check_enough_space(*curr_block, size)) {
			return curr_block;
		}
		curr_block = curr_block->body.links.next;
	}
	return NULL;
}

int check_enough_space(sf_block block, size_t required_size) {
	sf_header header = block.header;
	size_t block_size = header & ~(0xF);
	return (block_size >= required_size);
}




sf_block *expand_heap_to_fit(size_t size) {
	size_t new_size = 0;
	void *new_page_start;
	sf_block *block_start = sf_mem_end() - 8;
	sf_block *wilderness_list_head = &sf_free_list_heads[7];
	int prev_allocated;
	if (wilderness_list_head->body.links.next == wilderness_list_head) {
		prev_allocated = 1;
	} else {
		prev_allocated = 0;
	}
	sf_header header;
	while (new_size < size) {
		new_page_start = sf_mem_grow();
		if (new_page_start == NULL) {
			return NULL;
		}
		new_page_start -= 8; /* Account for 8 bytes of previous epilogue */
		header = create_header(PAGE_SZ, prev_allocated, 0);
		((sf_block *) new_page_start)->header = header;
		block_start = coalesce(new_page_start);
		new_size = (block_start->header) & ~(0xF);
		prev_allocated = 0;
	}
	return block_start;
}

sf_block *coalesce(sf_block *block) {
	sf_header header = block->header;
	int prev_allocated = (header & 0x2) >> 1;
	size_t block_size = header & ~(0xF);
	sf_block *block_start = block;
	sf_block *next_block = ((void *) block) + block_size;
	if (!prev_allocated) {
		sf_footer *prev_footer = ((void *) block) - 8;
		size_t prev_size = *prev_footer & ~(0xF);
		remove_from_free_list((void *) block - prev_size);
		block_size += prev_size;
		block_start = ((void *) block_start) - prev_size;
		prev_allocated = (*prev_footer & 0x2) >> 1;
	}
	sf_header next_header = next_block->header;
	int next_allocated = next_header & 0x1;
	if ((void *) next_block < (sf_mem_end() - 8) && !next_allocated) {
		remove_from_free_list(next_block);
		size_t next_size = next_header & ~(0xF);
		block_size += next_size;
		next_block = ((void *) next_block) + next_size;
	}
	sf_footer *block_footer = ((void *) next_block) - 8;
	sf_header new_header = create_header(block_size, prev_allocated, 0);
	(block_start->header) = new_header;
	*block_footer = new_header;
	sf_block *list_head = get_relevant_free_list_head(block_size, block_start);
	insert_into_free_list(block_start, list_head);
	return block_start;
}

void remove_from_free_list(sf_block *block) {
	sf_block *prev = block->body.links.prev;
	sf_block *next = block->body.links.next;
	prev->body.links.next = next;
	next->body.links.prev = prev;
}


int power(int base, int exp) {
	if (exp < 0) {
		return 0;
	}
	int result = 1;
	for (int i = 0; i < exp; i++) {
		result *= base;
	}
	return result;
}


void allocate_block(sf_block *block, size_t size) {
	sf_header header = block->header;
	header |= THIS_BLOCK_ALLOCATED;
	header &= 0xF; /* Mask off the size bits */
	header |= size;
	block->header = header;
	remove_from_free_list(block);
	void *next_block = ((void *) block) + size;
	set_prev_allocation_flag((sf_block *) next_block, 1);
}

void set_prev_allocation_flag(sf_block *block, int prev_allocation) {
	if ((void *) block >= sf_mem_end() - 8) { /* If in epilogue or out of bounds */
		return;
	}
	sf_header header = block->header;
	sf_header new_header = header & ~(0x2);
	prev_allocation <<= 1;
	new_header = new_header | prev_allocation;
	block->header = new_header;
	int allocated = header & 0x1;
	if (!allocated) { /* If block is not allocated, must set new footer too */
		size_t block_size = header & ~(0xF);
		sf_footer *footer = ((void *) block) + block_size - 8;
		*footer = new_header;
	}
}









void sf_free(void *pp) {
	if (!valid_pointer(pp)) {
		abort();
	}
	sf_block *block = (sf_block *) (pp - 8); /* Go to header of block */
	block->header = block->header & ~(THIS_BLOCK_ALLOCATED);
	sf_block *new_block = coalesce(block);
	size_t new_block_size = (new_block->header) & ~(0xF);
	set_prev_allocation_flag((void *) new_block + new_block_size, 0);
    return;
}

int valid_pointer(void *pointer) {
	if (pointer == NULL) goto INVALID;
	if ((uintptr_t) pointer % 16 != 0) goto INVALID;
	sf_block *block = (sf_block *) (pointer - 8); /* Go to where header starts */
	sf_header header = block->header;
	size_t block_size = header & ~(0xF);
	if (block_size % 16 != 0 || block_size < 32) goto INVALID;
	if (!(header & THIS_BLOCK_ALLOCATED)) goto INVALID;
	if (((void *) block + block_size) > sf_mem_end() || ((void *) block + block_size + 8) > sf_mem_end()) goto INVALID;
	sf_footer *prev_footer = (void *) block - 8;
	int prev_allocated = (header & PREV_BLOCK_ALLOCATED) >> 1;
	if (!prev_allocated) {
		if ((*prev_footer & THIS_BLOCK_ALLOCATED) != prev_allocated) goto INVALID;
	}
	return 1;

	INVALID:
		return 0;
}










void *sf_realloc(void *pp, size_t rsize) {
	if (!valid_pointer(pp)) {
		sf_errno = EINVAL;
		return NULL;
	} else if (rsize == 0) {
		sf_free(pp);
		return NULL;
	}
	sf_block *block = pp - 8; /* Go to start of block */
	sf_header header = block->header;
	size_t block_size = header & ~(0xF);
	if (block_size - 8 < rsize) {
		pp = sf_realloc_larger(block, rsize);
	} else if (block_size - 8 > rsize) {
		pp = sf_realloc_smaller(block, rsize);
	}
	if (pp == NULL) {
		return NULL;
	}
    return pp;
}

void *sf_realloc_larger(sf_block *block, size_t rsize) {
	void *new_mem = sf_malloc(rsize);
	if (new_mem == NULL) {
		return NULL;
	}
	sf_header header = block->header;
	size_t payload_size = (header & ~(0xF)) - 8; /* Block size without header */
	void *payload_start = (void *) block + 8;
	memcpy(new_mem, payload_start, payload_size);
	sf_free(payload_start);
	return new_mem;
}

void *sf_realloc_smaller(sf_block *block, size_t rsize) {
	sf_header header = block->header;
	size_t block_size = header & ~(0xF);
	size_t new_block_size = calculate_aligned_block_size(rsize);
	size_t split_size = block_size - new_block_size;
	if (split_size >= 32) {
		sf_block *split_block_addr = ((void *) block) + new_block_size;
		sf_header header = create_header(split_size, 1, 0);
		split_block_addr->header = header;
		coalesce(split_block_addr);
		set_prev_allocation_flag((void *) split_block_addr + (split_block_addr->header & ~(0xF)), 0);
		block->header &= 0xF;
		block->header |= new_block_size;
	}
	return ((void *) block + 8); /* Return start of payload */
}








void *sf_memalign(size_t size, size_t align) {
	if (align < 32 || !is_power_of_two(align)) {
		sf_errno = EINVAL;
		return NULL;
	} else if (size == 0) {
		return NULL;
	}
	size_t new_size = size + align + 32 + 8;
	void *allocated = sf_malloc(new_size);
	void *aligned_addr = allocated;
	if ((uintptr_t) allocated % align != 0) {
		aligned_addr = find_address_with_alignment(allocated, align);
		size_t free_space = aligned_addr - allocated;
		sf_block *block = allocated - 8;
		size_t allocated_size = block->header & ~(0xF);
		block->header &= 0xF;
		block->header |= free_space;
		block = aligned_addr - 8;
		block->header = create_header(allocated_size - free_space, 0, 1);
		sf_free(allocated);
	}
	return sf_realloc(aligned_addr, size);
}

int is_power_of_two(size_t val) {
	size_t result = 1;
	while (result < val) {
		result *= 2;
	}
	return result == val;
}


void *find_address_with_alignment(void *addr, size_t align) {
	void *new_addr = addr;
	while (((uintptr_t) new_addr % align != 0) || (new_addr - addr < 32)) {
		new_addr += 8;
	}
	return new_addr;
}

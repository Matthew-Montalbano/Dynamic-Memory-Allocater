#ifndef MY_SFMM_H
#define MY_SFMM_H
#endif

void initialize_heap();
void allocate_prologue();
void allocate_epilogue();
sf_header create_header(size_t block_size, int prv_alloc, int alloc);
void create_free_block(size_t block_size, int prv_alloc, sf_block *block_address);
void insert_into_free_list(sf_block *block, sf_block *list_head);
void initialize_free_lists();
size_t calculate_aligned_block_size(size_t size);
sf_block *find_free_block(size_t size);
sf_block *search_free_list(sf_block *head, size_t size);
int check_enough_space(sf_block block, size_t required_size);
sf_block *coalesce(sf_block *block);
void remove_from_free_list(sf_block *block);
sf_block *expand_heap_to_fit(size_t size);
sf_block *split_block(sf_block *block);
void allocate_block(sf_block *block, size_t size);
sf_block *get_relevant_free_list_head(size_t size, void *block);
void set_prev_allocation_flag(sf_block *block, int prev_allocation);


int valid_pointer(void *pp);



void *sf_realloc_larger(sf_block *block, size_t rsize);
void *sf_realloc_smaller(sf_block *block, size_t rsize);



int is_power_of_two(size_t val);
void *find_address_with_alignment(void *addr, size_t align);


int power(int base, int exp);
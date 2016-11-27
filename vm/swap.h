#include <bitmap.h>
#include "devices/block.h"

void init_swap ();

void swap_read (size_t swap_num, void *kpage);

void swap_write (size_t swap_num, void *kpage);

size_t get_swap_num ();


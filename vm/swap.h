#include <bitmap.h>
#include "devices/block.h"

void init_swap ();

bool swap_read (int swap_num, void *kpage);

bool swap_write (int swap_num, void *kpage);

int get_swap_num ();

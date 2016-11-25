#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"

struct bitmap *swap_table;
struct block *swap_block;

void init_swap ()
{
	swap_block = block_get_role (BLOCK_SWAP);
	size_t size = block_size (swap_block) * BLOCK_SECTOR_SIZE / PGSIZE;
	swap_table = bitmap_create (size);
	bitmap_set_all (swap_table, 0);
}

bool swap_read (int swap_num, void *kpage)
{
	int i;
	for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++)
	{
		block_read (swap_block, swap_num * PGSIZE / BLOCK_SECTOR_SIZE + i, kpage + i * BLOCK_SECTOR_SIZE);
	}
	bitmap_flip(swap_table, swap_num);
}

bool swap_write (int swap_num, void *kpage)
{
	int i;
	for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++)
	{
		block_write (swap_block, swap_num * PGSIZE/BLOCK_SECTOR_SIZE + i, kpage + i * BLOCK_SECTOR_SIZE);
	}
}

int get_swap_num ()
{
	return bitmap_scan_and_flip (swap_table, 0, 1, 0);
}
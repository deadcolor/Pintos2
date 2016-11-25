#include "vm/page.h"
#include "vm/swap.h"
#include <list.h>
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/thread.h"

struct supplement_page *get_sp (void *upage, struct thread *thread)
{
	struct list_elem *e;
	struct supplement_page *sp;
	
	for (e = list_begin (&thread->spt); e != list_end (&thread->spt); e = list_next(e))
	{
		sp = list_entry (e, struct supplement_page, elem);
		if(sp->upage == upage)
		{
		//	ASSERT(0);
			break;
		}
		sp = NULL;
	}
	if(sp != NULL)
	{
//		printf ("hi!\n");
		return sp;
	}
	return NULL;
}

bool create_sp (void *upage, struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int type)
{
	struct supplement_page *sp = malloc(sizeof(struct supplement_page));
	if(sp == NULL)
		return false;
	sp->upage = upage;
	sp->type = type;
	if(type == 1 || type == 2)//1=excutable file, 2=mmap, 3=normal, 4=swap
	{
		sp->file = file;
		sp->ofs = ofs;
		sp->read_bytes = read_bytes;
		sp->zero_bytes = zero_bytes;
		sp->writable = writable;
	}
	list_push_back (&thread_current ()->spt, &sp->elem);
	return true;
}

bool load_sp (void *upage)
{
	struct supplement_page *sp = get_sp (upage, thread_current ());
	if (sp == NULL)
	{
//		printf ("dye here?\n");
		return false;
	}
	void *kpage = make_frame_upage (upage);
	if(kpage == NULL)
		return false;
//	sp->kpage = kpage;
	if (sp->type == 1 || sp->type == 2)
	{
		file_seek (sp->file, sp->ofs);
		if (file_read (sp->file, kpage, sp->read_bytes) != (int) sp->read_bytes)
		{
			delete_frame (kpage);
			return false;
		}
		memset (kpage + sp->read_bytes, 0, sp->read_bytes);
		if (!install_page (sp->upage, kpage, sp->writable))
		{
			delete_frame (kpage);
			return false;
		}
		return true;
	}
	else if (sp->type == 4)
	{
		if (swap_read(sp->sector_num, kpage))
		{
			if(!install_page (sp->upage, kpage, sp->writable))
			{
				delete_frame (kpage);
				return false;
			}
			return true;
		}
		delete_frame (kpage);
		return false;
	}
	delete_frame (kpage);
	return false;
}

bool evict_sp (void *upage, struct thread *thread)
{
	struct supplement_page *sp = get_sp (upage, thread);
	void *kpage = pagedir_get_page (thread->pagedir, upage);
	if (sp->type == 2)
	{
		if (pagedir_is_dirty (thread->pagedir, upage))
		{
			pagedir_set_dirty (thread->pagedir, upage, false);
			file_seek (sp->file, sp->ofs);
			file_write (sp->file, kpage, sp->read_bytes);
		}
	}
	else if (sp->type == 3 || sp->type == 4)
	{
		sp->type = 4;
		sp->sector_num = get_swap_num ();
		if (sp->sector_num == BITMAP_ERROR)
			return false;
		if(!swap_write(sp->sector_num, kpage))
			return false;
	}
	pagedir_clear_page (thread->pagedir, upage);
	return true;
}

bool close_all ()
{

}


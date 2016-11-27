#include "vm/page.h"
#include "vm/swap.h"
#include <list.h>
#include <stdio.h>
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "vm/swap.h"
#include "threads/vaddr.h"

struct supplement_page *get_sp (void *upage, struct thread *thread)
{
	struct list_elem *e;
	struct supplement_page *sp;
	
	for (e = list_begin (&thread->spt); e != list_end (&thread->spt); e = list_next(e))
	{
		sp = list_entry (e, struct supplement_page, elem);
		if(sp->upage == upage)
		{
			break;
		}
		sp = NULL;
	}
	if(sp != NULL)
	{
		return sp;
	}
	return NULL;
}

bool create_sp (void *upage, struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int type)
{
//	printf ("create sp type: %d\n",type);
	struct supplement_page *sp = malloc(sizeof(struct supplement_page));
	if(sp == NULL)
		return false;
	sp->upage = upage;
	sp->type = type;
	sp->lock = false;
	if(type == 1 || type == 2)//1=excutable file, 2=mmap, 3=normal, 4=swap
	{
		sp->file = file;
		sp->ofs = ofs;
		sp->read_bytes = read_bytes;
		sp->zero_bytes = zero_bytes;
		sp->writable = writable;
	}
	if(type == 3)
	{
		sp->writable = writable;
	}
	list_push_back (&thread_current ()->spt, &sp->elem);
	return true;
}

bool load_sp (void *upage)
{
//	printf ("load start\n");
	struct supplement_page *sp = get_sp (upage, thread_current ());
	if (sp == NULL)
	{
		return false;
	}
	sp->lock = true;
	void *kpage = make_frame_upage (upage);
	if(kpage == NULL)
	{
		return false;
	}
	if (sp->type == 1 || sp->type == 2)
	{
		file_seek (sp->file, sp->ofs);
		if (file_read (sp->file, kpage, sp->read_bytes) != (int) sp->read_bytes)
		{
			delete_frame (kpage);
			return false;
		}
		memset (kpage + sp->read_bytes, 0, sp->zero_bytes);
		if (!install_page (sp->upage, kpage, sp->writable))
		{
			delete_frame (kpage);
			return false;
		}
		sp->lock = false;
		return true;
	}
	else if (sp->type == 3)
	{
		if (!install_page (sp->upage, kpage, sp->writable))
		{
			delete_frame (kpage);
			return false;
		}
		memset (kpage, 0, PGSIZE);
		sp->lock = false;
		return true;
	}
	else if (sp->type == 4)
	{
		if(install_page (sp->upage, kpage, sp->writable))
		{
			swap_read(sp->sector_num, kpage);
			sp->lock = false;
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
//	printf ("evict_sp start\n");
	struct supplement_page *sp = get_sp (upage, thread);
	void *kpage = pagedir_get_page (thread->pagedir, upage);
	if (sp->type == 1)
	{
		if (pagedir_is_dirty (thread->pagedir, upage))
		{
			pagedir_set_dirty (thread->pagedir, upage, false);
			sp->type = 4;
			sp->sector_num = get_swap_num ();
			if (sp->sector_num == BITMAP_ERROR)
				return false;
			swap_write(sp->sector_num, kpage);
		}
	}
	else if (sp->type == 2)
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
		{
			return false;
		}
		swap_write(sp->sector_num, kpage);
	}
	else
	{
		return false;
	}
	pagedir_clear_page (thread->pagedir, upage);
	return true;
}

bool stack_growth (void *upage)
{
//	printf ("stack growth\n");
	if (!create_sp (upage, NULL, 0, 0, 0, true, 3))
		return false;
	
	if (!load_sp (upage))
		return false;

	return true;
}

void close_all_page ()
{
	struct list_elem *e;
	struct supplement_page *sp;
	for ( e = list_begin (&thread_current ()->spt); e != list_end (&thread_current ()->spt); )
	{
		sp = list_entry (e, struct supplement_page, elem);
		if(sp->type == 4)
		{
			if (pagedir_get_page (thread_current ()->pagedir, sp->upage) == NULL)
			{
				free_swap_table (sp->sector_num);	
			}
		}
		e = list_next (e);
		list_remove (&sp->elem);
		pagedir_clear_page (thread_current ()->pagedir, sp->upage);
		free (sp);
	}	
}


void close_file(void *upage)
{
	struct supplement_page *sp = get_sp(upage,thread_current());	
	ASSERT(sp!= NULL)
    
	//Check if that file is already written
	bool dirted = 0;
	if(sp->writable == true && pagedir_is_dirty(thread_current()->pagedir,sp->upage) && sp->type == 2)
		dirted = 1;
	
	void *kpage = get_frame_upage(upage,thread_current());
	if(kpage == NULL)
	{
		bool success = load_sp(upage);
		if(success)
			kpage = get_frame_upage(upage,thread_current());
		else
			ASSERT(0);
	}

	if(dirted)
	{
		file_seek(sp->file,sp->ofs);
		file_write(sp->file, kpage, sp->read_bytes);	
	};
	delete_frame(kpage);

	pagedir_clear_page(thread_current()->pagedir, sp->upage);
	list_remove(&sp->elem); 
	free(sp);

}

/* Unmap mmap_file.return 1 if success,  return 0 if fail*/
bool unmap_file(int mapid)
{
	//Find mmap_files in thread_current
	struct thread *t = thread_current();
	struct list_elem *e = NULL;
	for(e = list_begin (&t->mmap_list); e!= list_end(&t->mmap_list);)
	{
		struct mmap_file *file = list_entry(e, struct mmap_file, elem);
		
		e = list_next(e);
		if(file->mapid == mapid)
		{
			list_remove(&file->elem);
			close_file(file->upage);
			free(file);		
		}
		else
		{
		}
	}
	//If fail to find mmap_file, ERROR
	if(e == NULL)
		return 0;
	else
		return 1;
}

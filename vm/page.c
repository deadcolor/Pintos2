#include "vm/page.h"
#include "vm/swap.h"
#include <list.h>
#include <stdio.h>
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
	if(type == 3)
	{
		sp->writable = writable;
	}
	list_push_back (&thread_current ()->spt, &sp->elem);
	return true;
}

bool load_sp (void *upage)
{
	int j = 0;
//	printf ("load start %d\n",j++);
	struct supplement_page *sp = get_sp (upage, thread_current ());
//	printf ("load %d\n", j++);
	if (sp == NULL)
	{
//		printf ("dye here?\n");
		return false;
	}
//	printf ("load %d\n", j++);
	void *kpage = make_frame_upage (upage);
//	printf ("load %d\n", j++);
	if(kpage == NULL)
	{
//	printf ("load dye?%d\n", j++);
		return false;
	}
//	sp->kpage = kpage;
	if (sp->type == 1 || sp->type == 2)
	{
		file_seek (sp->file, sp->ofs);
		if (file_read (sp->file, kpage, sp->read_bytes) != (int) sp->read_bytes)
		{
			delete_frame (kpage);
//	printf ("load dye here %d\n", j++);
			return false;
		}
		memset (kpage + sp->read_bytes, 0, sp->read_bytes);
		if (!install_page (sp->upage, kpage, sp->writable))
		{
//	printf ("load no dye%d\n", j++);
			delete_frame (kpage);
			return false;
		}
		return true;
	}
	else if (sp->type == 3)
	{
//	printf ("load type3 %d\n", j++);
		if (!install_page (sp->upage, kpage, sp->writable))
		{
//	printf ("load type3 dye %d\n", j++);
			delete_frame (kpage);
			return false;
		}
		return true;
	}
	else if (sp->type == 4)
	{
//	printf ("load type4 %d\n", j++);
	/*	if (swap_read(sp->sector_num, kpage))
		{
			if(!install_page (sp->upage, kpage, sp->writable))
			{
//			printf ("load type4 dye %d\n", j++);
				delete_frame (kpage);
				return false;
			}
			return true;
		}*/
		if(install_page (sp->upage, kpage, sp->writable))
		{
			if (swap_read(sp->sector_num, upage))
				return true;
		}
//	printf ("load type4 dye2 %d\n", j++);
		delete_frame (kpage);
		return false;
	}
//	printf ("load just dye %d\n", j++);
	delete_frame (kpage);
	return false;
}

bool evict_sp (void *upage, struct thread *thread)
{
//	printf ("evict_sp start\n");
	struct supplement_page *sp = get_sp (upage, thread);
	void *kpage = pagedir_get_page (thread->pagedir, upage);
	if (sp->type == 2)
	{
//		printf ("evict type 2\n");
		if (pagedir_is_dirty (thread->pagedir, upage))
		{
			pagedir_set_dirty (thread->pagedir, upage, false);
			file_seek (sp->file, sp->ofs);
			file_write (sp->file, kpage, sp->read_bytes);
		}
	}
	else if (sp->type == 3 || sp->type == 4)
	{
//		printf ("evict type 3, 4 \n");
		sp->type = 4;
		sp->sector_num = get_swap_num ();
		if (sp->sector_num == BITMAP_ERROR)
			return false;
		if(!swap_write(sp->sector_num, upage))
			return false;
	}
	pagedir_clear_page (thread->pagedir, upage);
	return true;
}

bool stack_growth (void *upage)
{
	if (!create_sp (upage, NULL, 0, 0, 0, true, 3))
		return false;
	
	if (!load_sp (upage))
		return false;

	return true;
}

bool close_all ()
{
	
}


void close_file(void *upage)
{
	struct supplement_page *sp = get_sp(upage,thread_current());	
	ASSERT(sp!= NULL)
    
	//Check if that file is already written
	bool dirted = 0;
	if(sp->writable == true && pagedir_is_dirty(thread_current()->pagedir,sp->upage))
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
	//TODO:: frame_clear? close file?
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

#include "vm/frame.h"
#include "vm/page.h"
#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"

struct list frame_table;

struct frame *clock_frame;

struct frame
{
	void *kpage;
	void *upage;
	struct thread *thread;
	struct list_elem elem;
};

void frame_init ()
{
	list_init (&frame_table);
}

struct frame *get_frame (void *kpage)
{
	struct list_elem *e;
	struct frame *f;
	for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
	{
		f = list_entry (e, struct frame, elem);
		if(f->kpage == kpage)
			break;
		f = NULL;
	}
	return f;
}

void *make_frame ()
{
	void *kpage = palloc_get_page (PAL_USER);
	if (kpage == NULL)
	{
		if(!frame_evict ())
			return NULL;
		kpage = palloc_get_page (PAL_USER);
	}
	return kpage;
}

void *make_frame_upage (void *upage)
{
	void *kpage = make_frame ();
	if (kpage == NULL)
		return NULL;
	
	struct frame *frame = malloc(sizeof(struct frame));
	frame->kpage = kpage;
	frame->upage = upage;
	frame->thread = thread_current ();
	list_push_back (&frame_table, &frame->elem);
	return kpage;	
}

void delete_frame (void *kpage)
{
	struct frame *f = get_frame (kpage);
	palloc_free_page (kpage);
	list_remove (&f->elem);
	free (f);
}

bool frame_evict ()
{
	struct list_elem *e;
	struct frame *f;

	e = &clock_frame->elem;
	do
	{
		f = list_entry (e, struct frame, elem);
		if (pagedir_is_accessed (f->thread->pagedir, f->upage))
			pagedir_set_accessed (f->thread->pagedir, f->upage, false);
		else
		{
			if (e == list_end (&frame_table))
				e = list_begin (&frame_table);
			else
				e = list_next;
			clock_frame = list_entry (e, struct frame, elem);

			evict_sp (f->upage, f->thread);
			delete_frame (f->kpage);
			return true;
		}
		if (e == list_end (&frame_table))
			e = list_begin (&frame_table);
		else
			e = list_next (e);

	}while (f != clock_frame);
	/*for (e = list_begin (&frame_table), i = 0; e != list_end (&frame_table); e = list_next (e))
	{
		if (i>=clock_num)
		f = list_entry (e, struct frame, elem);
			
	}*/
	return false;
}



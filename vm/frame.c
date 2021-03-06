#include "vm/frame.h"
#include "vm/page.h"
#include <list.h>
#include <stdio.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"

struct list frame_table;

struct frame *clock_frame;

struct frame
{
	void *kpage;
	void *upage;
	struct thread *thread;
	struct list_elem elem;
};

struct lock frame_lock;

void frame_init ()
{
	list_init (&frame_table);
	lock_init (&frame_lock);
	clock_frame = NULL;
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
//	printf ("make_frame start\n");
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
	lock_acquire (&frame_lock);
	void *kpage = make_frame ();
	if (kpage == NULL)
	{
		lock_release (&frame_lock);
		return NULL;
	}
	struct frame *frame = malloc(sizeof(struct frame));
	frame->kpage = kpage;
	frame->upage = upage;
	frame->thread = thread_current ();
	list_push_back (&frame_table, &frame->elem);
	if (clock_frame == NULL)
		clock_frame = frame;
	lock_release (&frame_lock);
	return kpage;	
}

void delete_frame (void *kpage)
{
	struct frame *f = get_frame (kpage);
	palloc_free_page (kpage);
	list_remove (&f->elem);
	free (f);
}

void close_all_frame ()
{
	struct list_elem *e;
	struct frame *f;
	for ( e = list_begin (&frame_table); e != list_end (&frame_table); )
	{
		f = list_entry (e, struct frame, elem);
		e = list_next (e);
		if (f->thread == thread_current ())
		{
			list_remove (&f->elem);
			palloc_free_page (f->kpage);
			free (f);
		}
	}
}

bool frame_evict ()
{
	struct list_elem *e;
	struct frame *f;
	if (clock_frame == NULL)
		e= list_begin (&frame_table);
	else
		e = &clock_frame->elem;
	f = list_entry (e, struct frame, elem);
	do
	{
		struct supplement_page * sp = get_sp (f->upage, f->thread);
		if (sp->lock)
		{
			e = list_next (e);
			if (e == list_end (&frame_table))
				e = list_begin (&frame_table);
		}
		else if (pagedir_is_accessed (f->thread->pagedir, f->upage))
		{
			pagedir_set_accessed (f->thread->pagedir, f->upage, false);
			e = list_next (e);
			if (e == list_end (&frame_table))
			{
				e = list_begin (&frame_table);
			}
		}
		else
		{
			e = list_next (e);
			if ( e == list_end (&frame_table))
			{
				e = list_begin (&frame_table);
			}
			if (evict_sp (f->upage, f->thread))
			{
				delete_frame (f->kpage);
				clock_frame = list_entry (e, struct frame, elem);
				return true;
			}
		}
		f = list_entry (e, struct frame, elem);
	}while (true/*f != clock_frame*/);
	return false;
}



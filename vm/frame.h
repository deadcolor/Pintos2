#include <list.h>
#include "threads/thread.h"

/*struct frame
{
	void *kpage; //physicall memory
	void *upage; //virtual memory
	struct thead; //thread which get this frame
	struct list_elem elem;
};*/

void frame_init ();

void *make_frame ();

void *make_frame_upage (void *upage);

bool frame_evict ();



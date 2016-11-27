#include <list.h>
#include "filesys/file.h"
#include "threads/thread.h"

struct supplement_page
{
	void *upage;
	int type;
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	size_t sector_num;
	bool lock;
	struct list_elem elem;
};

struct supplement_page *get_sp (void *upage, struct thread *thread);

bool create_sp (void *upage, struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int type);

bool load_sp (void *upage);

void close_all_page ();


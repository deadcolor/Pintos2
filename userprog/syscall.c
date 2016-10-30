#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "list.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* check whether address is under PHYS_BASE(in user stack)
*/
static bool
is_valid_address(void *address)
{
  //TODO : not completed
  if(!is_user_vaddr(address) || address < (void*)0x08048000)
  {
	return 0;
  }
	return 1;		
}

struct thread_file *get_file_list(int fd)
{
	struct list_elem *e;
	struct thread_file *F;
	struct thread_file *f;
	F = NULL;
	for( e = list_begin (&thread_current ()->file_list); e != list_end (&thread_current ()->file_list); e = list_next (e))
	{
		f = list_entry (e, struct thread_file, elem);
		if (f->fd == fd)
		{
			F = f;
			break;
		}
	}
	return F;
}


static void
syscall_handler (struct intr_frame *f) 
{
	if(!is_valid_address((void *) f->esp))
	{
		thread_current ()->exit_status = -1;
		thread_exit();
	}

	int call_number = *(int *)f->esp;

	
	switch(call_number)
	{
		case SYS_HALT:
			shutdown_power_off();
			break;
		case SYS_EXIT:
		{
			int status = *(int *)(f->esp + 4);
			thread_current ()->exit_status = status;
			thread_exit();
			break;
		}
		case SYS_EXEC:
		{
			char *str = *(char **)(f->esp + 4);
			f->eax = process_execute (str);
			//not implemented
			break;
		}
		case SYS_WAIT:
		{
			int pid;
			pid = *(int *)(f->esp + 4);
			f->eax = process_wait(pid);
			//not implemented
			break;
		}
		case SYS_CREATE:
		{
			char *str = *(char **)(f->esp + 4);//file name
			int size = *(int *)(f->esp + 8);//size
		//	acquire_filesys_lock ();
			f->eax = filesys_create (str, size);
		//	release_filesys_lock ();
			break;
		}
		case SYS_REMOVE:
		{
			char *str = *(char **)(f->esp + 4);//file name
			filesys_remove (str);
			break;
		}
		case SYS_OPEN:
		{
			char *str = *(char **)(f->esp + 4);//file name
			struct file *file = filesys_open (str);
			if(file == NULL)
				f->eax = -1;
			else
			{
				struct thread_file *thread_file = malloc(sizeof(struct thread_file));
				thread_file->fd = thread_current()->fd_count++;
				thread_file->file = file;
				list_push_back (&thread_current()->file_list, &thread_file->elem);
				f->eax = thread_file->fd;
			}
			break;
		}
		case SYS_FILESIZE:
		{
			int fd = *(int *)(f->esp + 4);
			if (get_file_list (fd) == NULL)
				f->eax = -1;
			else
			{
				f->eax = file_length(get_file_list (fd)->file);
			}
			break;
		}
		case SYS_READ:
		{
			//TODO:: check valid pointer fd, buffer, size
			int fd = *(int *)(f->esp + 4);
			void *buffer = *(char **)(f->esp + 8);
			int size = *(int *)(f->esp + 12);
			if (get_file_list (fd) == NULL)
				f->eax = -1;
			else
				f->eax = file_read (get_file_list (fd)->file, buffer, size);
			break;
		}
		case SYS_WRITE:
		{
			int fd = *(int *)(f->esp + 4);
			void *buffer = *(char **)(f->esp + 8);
			int size = *(int *)(f->esp + 12);
			if (fd == 1)
			{
				putbuf ((char *)buffer, (size_t)size);
				f->eax = -1;
			}
			else
			{
				if (get_file_list (fd) == NULL)
					f->eax = -1;
				else
					f->eax = file_write(get_file_list (fd)->file, buffer, size);
			}
			break;
		}
		case SYS_SEEK:
		{
			int fd = *(int *)( f->esp + 4);
			unsigned position = *(unsigned *)(f->esp + 8);
			file_seek(get_file_list(fd)->file, position);
			break;
		}	
		case SYS_TELL:
		{
			int fd = *(int *)( f->esp + 4);
			f->eax = file_tell(get_file_list(fd)->file);
			break;
		}
		case SYS_CLOSE:
		{
			int fd = *(int *)( f->esp + 4);
			struct thread_file *thread_file = get_file_list (fd);
			if( thread_file != NULL)
			{
				file_close (thread_file->file);
				list_remove (&thread_file->elem);	
				free (thread_file);
			}
			break; 
		}
	}			
}

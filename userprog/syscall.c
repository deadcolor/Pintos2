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
#include "threads/synch.h"

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
  if(!is_user_vaddr(address))
	return 0;

  //Check if bad_ptr input
  uint32_t *ptr = pagedir_get_page(thread_current()->pagedir, address);
  if(!ptr)
	return 0;

  else
	return 1;		
}

/* check valid string */
static bool
is_string(const char *string)
{
  //If string is null
  if(string == NULL)
	return 0;
  if(!is_valid_address(string))
	return 0;

  //Check if bad_ptr input
  uint32_t *ptr = pagedir_get_page(thread_current()->pagedir, string);
  if(!ptr)
	return 0;
	
	return 1;
}

struct thread_file *get_file_list (int fd)
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
		        if(!is_valid_address((void *)f->esp +4))
				thread_current ()->exit_status = -1;
			else {		
				int status = *(int *)(f->esp + 4);
				thread_current ()->exit_status = status;
			}
			thread_exit();
			break;
		}
		case SYS_EXEC:
		{
			const char *str = *(char **)(f->esp + 4);
			if(!is_valid_address(str))
			{
   				thread_current ()->exit_status = -1;
                                thread_exit();
			}
				f->eax = process_execute (str);
			break;
		}
		case SYS_WAIT:
		{
			int pid;
			pid = *(int *)(f->esp + 4);
			f->eax = process_wait(pid);
			break;
		}
		case SYS_CREATE:
		{
		        if(!is_valid_address((void *)f->esp +4) || !is_valid_address((void *)f->esp +8)|| !is_string(*(char **)(f->esp +4))){
				thread_current ()->exit_status = -1;
				thread_exit();
			}
			char *str = *(char **)(f->esp + 4);//file name
			int size = *(int *)(f->esp + 8);//size
			file_lock_acquire ();
			f->eax = filesys_create (str, size);
			file_lock_release ();
			break;
		}
		case SYS_REMOVE:
		{
			char *str = *(char **)(f->esp + 4);//file name
			file_lock_acquire ();
			filesys_remove (str);
			file_lock_release ();
			break;
		}
		case SYS_OPEN:
		{
			if(!is_valid_address((void *)f->esp +4) || !is_string(*(char **)(f->esp +4))){
                                thread_current ()->exit_status = -1;
                                thread_exit();
                        }
			
			char *str = *(char **)(f->esp + 4);//file name
			file_lock_acquire ();
			struct file *file = filesys_open (str);
			file_lock_release ();
			if(file == NULL)
				f->eax = -1;
			else
			{
				struct thread_file *thread_file = malloc(sizeof(struct thread_file));
				lock_acquire (&thread_current ()->fd_lock);
				thread_file->fd = thread_current ()->fd_count++;
				lock_release (&thread_current ()->fd_lock);
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
				file_lock_acquire ();
				f->eax = file_length(get_file_list (fd)->file);
				file_lock_release ();
			}
			break;
		}
		case SYS_READ:
		{
			int fd = *(int *)(f->esp + 4);
			void *buffer = *(char **)(f->esp + 8);
			int size = *(int *)(f->esp + 12);
			
			if(!is_valid_address(buffer)){
				thread_current()->exit_status = -1;
				thread_exit();
			}
			if (get_file_list (fd) == NULL)
				f->eax = -1;
			else
			{
				file_lock_acquire ();
				f->eax = file_read (get_file_list (fd)->file, buffer, size);
				file_lock_release ();
			}
			break;
		}
		case SYS_WRITE:
		{
			int fd = *(int *)(f->esp + 4);
			void *buffer = *(char **)(f->esp + 8);
			int size = *(int *)(f->esp + 12);
			
			if(!is_valid_address(buffer)){
                                thread_current()->exit_status = -1;
                                thread_exit();
                        }
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
				{
					file_lock_acquire ();
					f->eax = file_write(get_file_list (fd)->file, buffer, size);
					file_lock_release ();
				}
			}
			break;
		}
		case SYS_SEEK:
		{
			int fd = *(int *)( f->esp + 4);
			unsigned position = *(unsigned *)(f->esp + 8);
			file_lock_acquire ();
			file_seek(get_file_list(fd)->file, position);
			file_lock_release ();
			break;
		}	
		case SYS_TELL:
		{
			int fd = *(int *)( f->esp + 4);
			file_lock_acquire ();
			f->eax = file_tell(get_file_list(fd)->file);
			file_lock_release ();
			break;
		}
		case SYS_CLOSE:
		{
			int fd = *(int *)( f->esp + 4);
			struct thread_file *thread_file = get_file_list (fd);
			if (thread_file != NULL)
			{
				file_lock_acquire ();
				file_close (thread_file->file);
				file_lock_release ();
				list_remove (&thread_file->elem);	
				free (thread_file);
			}
			break; 
		}
		case SYS_MMAP:
		{
			int fd = *(int *) (f->esp +4);
			void *addr = *(void **)(f->esp +8);

			//Check whether file descriptor is invalid
			if(fd == 0 || fd == 1 || fd < 0)
				f->eax = -1;
			//Check addr is not null & page-aligned
			if( addr == NULL || pg_round_down(addr) != addr  || addr == 0x0)
				f->eax = -1;	
	
			break;
		}
		case SYS_MUNMAP:
		{
			
			break;
		}
	}			
}

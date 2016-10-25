#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	//TODO: We should check whether (int *)f->esp is valid pointer or not
	int call_number = *(int *)f->esp;

	switch(call_number)
	{
		case SYS_HALT:
			shutdown_power_off();
			break;
		case SYS_EXIT:
			//not implemented
			break;
		case SYS_EXEC:
			//not implemented
			break;
		case SYS_WAIT:
			//not implemented
			break;
		case SYS_CREATE:
			//not implemented
			break;
		case SYS_REMOVE:
			//not implemented
			break;
		case SYS_OPEN:
			//not implemented
			break;
		case SYS_FILESIZE:
			//not implemented
			break;
		case SYS_READ:
			//not implemented
			break;
		case SYS_WRITE:
			//not implemented
			break;
		case SYS_SEEK:
			//not implemented
			break;
		case SYS_TELL:
			//not implemented
			break;
		case SYS_CLOSE:
			//not implemented
			break;
	}			
}

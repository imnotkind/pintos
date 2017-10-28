#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *p = f->esp;
  //address safety checking needed !!!!!!!!!!!!!
  //is_user_vaddr(), pagedir_get_page() two tests required!
  int sysno = *p;
  printf ("system call! no %d\n",sysno);
  switch(sysno)
  {
    case SYS_HALT:                   /* Halt the operating system. */
      shutdown_power_off();
      break;

    case SYS_EXIT:                   /* Terminate this process. */
    {
      int status = *(int *)(p+1);
      //we must close all files opened by process! LATER
      //OR we must interact with parents MAYBE
      f->eax = status;
      printf("%s: exit(%d)\n",thread_current()->name, status);
      thread_exit();
      /* not reached */
    }
      break;

    case SYS_EXEC:                   /* Start another process. */
    case SYS_WAIT:                   /* Wait for a child process to die. */
    case SYS_CREATE:                 /* Create a file. */
    case SYS_REMOVE:                 /* Delete a file. */
    case SYS_OPEN:                   /* Open a file. */
    case SYS_FILESIZE:               /* Obtain a file's size. */
    case SYS_READ:                   /* Read from a file. */
    case SYS_WRITE:                  /* Write to a file. */
    {
      int fd = *(int *)(p+1);
      char * buffer = *(char **)(p+2);
      unsigned int size = *(int *)(p+3);
      //printf("fd : %d\n",fd);
      //printf("buffer : %s\n",buffer);
      //printf("size : %d\n",size);
      if(fd == 1)
        putbuf(buffer,(size_t)size); //too big size may be a problem, but i wont care for now. LATER
      f->eax = size;
    }
      break;

    case SYS_SEEK:                   /* Change position in a file. */
    case SYS_TELL:                   /* Report current position in a file. */
    case SYS_CLOSE:                  /* Close a file. */

    /* Project 3 and optionally project 4. */
    case SYS_MMAP:                   /* Map a file into memory. */
    case SYS_MUNMAP:                 /* Remove a memory mapping. */

    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
    case SYS_MKDIR:                  /* Create a directory. */
    case SYS_READDIR:                /* Reads a directory entry. */
    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
    case SYS_INUMBER:                 /* Returns the inode number for a fd. */
    default:
      break;
  }
}

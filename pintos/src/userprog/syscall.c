#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <list.h>
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

struct  flist_elem
{
  int fd; //file descriptor
  struct file *fp; //file pointer
  struct list_elem elem;
};

static int fd_next = 3;

static void syscall_handler (struct intr_frame *); //don't move this to header
struct flist_elem* find_flist_elem(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *p = f->esp;
  //address safty check with is_user_vaddr() and pagedir_get_page()
  check_addr_safe(p);

  int sysno = *p;
  
  //printf ("system call! no %d\n",sysno);
  switch(sysno)
  {
    case SYS_HALT:                   /* Halt the operating system. */
      shutdown_power_off();
      break;

    case SYS_EXIT:                   /* Terminate this process. */
    {
      check_addr_safe(p+1);
      int status = *(int *)(p+1);
      //we must close all files opened by process! LATER
      //OR we must interact with parents MAYBE
      sys_exit(status);
      break;
    }


    case SYS_EXEC:                   /* Start another process. */
    case SYS_WAIT:                   /* Wait for a child process to die. */
    {
      check_addr_safe(p+1);
      int pid = *(int *)(p+1);
      f->eax = process_wait(pid);
      break;
    }
      
    case SYS_CREATE:                 /* Create a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(*(p+1));
      check_addr_safe(p+2);
      char *file = *(char **)(p+1);
      unsigned initial_size = *(unsigned *)(p+2);

      //lock
      f->eax = filesys_create(file, initial_size);
      //unlock
      break;
    }
    case SYS_REMOVE:                 /* Delete a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(*(p+1));
      char *file = *(char **)(p+1);

      //lock
      f->eax = filesys_remove(file);
      //unlock
      break;
    }
    case SYS_OPEN:                   /* Open a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(*(p+1));
      char * file_name = *(char **)(p+1);
      //lock
      struct file* fp = filesys_open (file_name);
      //unlock
      if(!fp){
        f->eax = -1;
        break;
      }
      else{
        struct flist_elem *fe = (struct flist_elem*)malloc(sizeof(struct flist_elem));
        fe->fp = fp;
        fe->fd = fd_next++;
        list_push_back(&thread_current()->file_list, &fe->elem);
        f->eax = fe->fd;
      }
      break;
    }
      

    case SYS_FILESIZE:               /* Obtain a file's size. */
    case SYS_READ:                   /* Read from a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(p+2);
      check_addr_safe(*(p+2));
      check_addr_safe(p+3);
      int fd = *(int *)(p+1);
      void *buffer = *(void **)(p+2);
      unsigned size = *(unsigned *)(p+3);

      if (fd == STDIN_FILENO)
      {
        int i;
        uint8_t* local_buffer = (uint8_t *)buffer;
        for(i = 0; i < size; i++)
        {
          local_buffer[i] = input_getc();
        }
        f->eax = size;
      }
      else
      {
        //lock
        struct flist_elem *fe = find_flist_elem(fd);
        if (!fe)
          f->eax = -1;
        else
          f->eax = file_read(fe->fp, buffer, size);
        //unlock
      }
      
      break;
    }
    case SYS_WRITE:                  /* Write to a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(p+2);
      check_addr_safe(*(p+2));
      check_addr_safe(p+3);
      int fd = *(int *)(p+1);
      void * buffer = *(void **)(p+2);
      unsigned size = *(unsigned *)(p+3);
      //printf("fd : %d\n",fd);
      //printf("buffer : %s\n",buffer);
      //printf("size : %d\n",size);
      if(fd == 1)
        putbuf((char *)buffer,(size_t)size); //too big size may be a problem, but i wont care for now. LATER
      f->eax = size;
      break;
    }


    case SYS_SEEK:                   /* Change position in a file. */
    case SYS_TELL:                   /* Report current position in a file. */
    case SYS_CLOSE:                  /* Close a file. */
    {
      check_addr_safe(p+1);
      int fd = *(int *)(p+1);

      struct flist_elem *fe = find_flist_elem(fd);
      if(!fe)
      {
        //when file is not found
      }
      else
      {
        list_remove(&fe->elem);
        file_close(fe->fp);
        free(fe);
      }
      break;
    }

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

static void check_addr_safe(const void *vaddr)
{
  if (!vaddr || !is_user_vaddr(vaddr) || !pagedir_get_page(thread_current()->pagedir, vaddr))
  {
    sys_exit(-1);
  }
}

struct flist_elem* find_flist_elem(int fd)
{
  struct flist_elem *fe;
  struct thread *cur = thread_current();
  struct list_elem *e;
  
  for (e = list_begin(&cur->file_list); e != list_end(&cur->file_list); e = list_next(e))
  {
    fe = list_entry (e, struct flist_elem, elem);
    if (fe->fd == fd)
      return fe;
  }
  return NULL;
}

void sys_exit(int status)
{
  thread_current()->exit_code = status;
  printf("%s: exit(%d)\n",thread_current()->name, status);
  thread_exit();
  NOT_REACHED();
}
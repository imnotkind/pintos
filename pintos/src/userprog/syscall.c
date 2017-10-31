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
#include <string.h>
#include "threads/synch.h"

struct flist_pack
{
  int fd; //file descriptor
  struct file *fp; //file pointer
  struct list_elem elem;
};

struct lock filesys_lock;

static int fd_next = 3;

static void syscall_handler (struct intr_frame *); //don't move this to header
void check_addr_safe(const void *vaddr);
struct flist_pack* fd_to_flist_pack(int fd);
void sys_exit(int status);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *p = f->esp;
  //address safty check with is_user_vaddr() and pagedir_get_page()
  check_addr_safe(p);

  int sysno = *p;
  
  switch(sysno)
  {
    case SYS_HALT:                   /* Halt the operating system. */
      shutdown_power_off();
      NOT_REACHED();
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
    {
      check_addr_safe(p+1);
      check_addr_safe((void *)*(p+1));
      char *cmd_line = *(char **)(p+1);

      char *file_name = malloc(strlen(cmd_line)+1); //pure file name to put in filesys_open
      strlcpy(file_name,cmd_line,strlen(cmd_line)+1);
      char *save_ptr;
      file_name = strtok_r(file_name," ",&save_ptr);

      lock_acquire(&filesys_lock);
      struct file *fp = filesys_open(file_name);
      if(fp == NULL)
      {
        f->eax = -1;
      }
      else
      {
        file_close(fp);
        f->eax = process_execute(cmd_line);
      }
      lock_release(&filesys_lock);
      break;
    }
      
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
      check_addr_safe((void *)*(p+1));
      check_addr_safe(p+2);
      char *file = *(char **)(p+1);
      unsigned initial_size = *(unsigned *)(p+2);

      lock_acquire(&filesys_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release(&filesys_lock);
      break;
    }

    case SYS_REMOVE:                 /* Delete a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe((void *)*(p+1));
      char *file = *(char **)(p+1);

      lock_acquire(&filesys_lock);
      f->eax = filesys_remove(file);
      lock_release(&filesys_lock);
      break;
    }

    case SYS_OPEN:                   /* Open a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe((void *)*(p+1));
      char * file_name = *(char **)(p+1);

      lock_acquire(&filesys_lock);
      struct file* fp = filesys_open (file_name);
      lock_release(&filesys_lock);
      if(!fp){
        f->eax = -1;
        break;
      }
      else{
        struct flist_pack *fe = (struct flist_pack*)malloc(sizeof(struct flist_pack));
        fe->fp = fp;
        fe->fd = fd_next++;
        list_push_back(&thread_current()->file_list, &fe->elem);
        f->eax = fe->fd;
      }
      break;
    }
      

    case SYS_FILESIZE:               /* Obtain a file's size. */
    {
      check_addr_safe(p+1);
      int fd = *(int *)(p+1);
      f->eax = (uint32_t)file_length(fd_to_flist_pack(fd)->fp);
      break;
    }
      
    case SYS_READ:                   /* Read from a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(p+2);
      check_addr_safe((void *)*(p+2));
      check_addr_safe(p+3);
      int fd = *(int *)(p+1);
      void *buffer = *(void **)(p+2);
      unsigned size = *(unsigned *)(p+3);

      if (fd == 0)
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
        lock_acquire(&filesys_lock);
        struct flist_pack *fe = fd_to_flist_pack(fd);
        if (!fe){
          lock_release(&filesys_lock);
          f->eax = -1;
        }          
        else{
          f->eax = file_read(fe->fp, buffer, (off_t)size);
          lock_release(&filesys_lock);
        }
      }
      
      break;
    }

    case SYS_WRITE:                  /* Write to a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(p+2);
      check_addr_safe((void *)*(p+2));
      check_addr_safe(p+3);
      int fd = *(int *)(p+1);
      void * buffer = *(void **)(p+2);
      unsigned size = *(unsigned *)(p+3);

      if(fd == 1){
        putbuf((char *)buffer,(size_t)size); //too big size may be a problem, but i wont care for now. LATER
        f->eax = size;
      }
      else{
        lock_acquire(&filesys_lock);
        struct flist_pack *fe = fd_to_flist_pack(fd);
        if (!fe){
          lock_release(&filesys_lock);
          f->eax = -1;
        }          
        else{
          f->eax = file_write(fe->fp, buffer, (off_t)size);
          lock_release(&filesys_lock);
        }
      }
      break;
    }


    case SYS_SEEK:                   /* Change position in a file. */
    {
      check_addr_safe(p+1);
      check_addr_safe(p+2);
      int fd = *(int *)(p+1);
      unsigned position = *(unsigned *)(p+2);

      struct flist_pack *fe = fd_to_flist_pack(fd);
      lock_acquire(&filesys_lock);
      file_seek(fe->fp, (off_t)position);
      lock_release(&filesys_lock);
      break;
    }

    case SYS_TELL:                   /* Report current position in a file. */
    {
      break;
    }
      
    case SYS_CLOSE:                  /* Close a file. */
    {
      check_addr_safe(p+1);
      int fd = *(int *)(p+1);

      struct flist_pack *fe = fd_to_flist_pack(fd);
      if(!fe)
      {
        sys_exit(-1);
      }
      else
      {
        list_remove(&fe->elem);
        
        lock_acquire(&filesys_lock);
        file_close(fe->fp);
        lock_release(&filesys_lock);
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

void check_addr_safe(const void *vaddr)
{
  if (!vaddr || !is_user_vaddr(vaddr) || !pagedir_get_page(thread_current()->pagedir, vaddr))
  {
    sys_exit(-1);
  }
}

struct flist_pack* fd_to_flist_pack(int fd)
{
  struct flist_pack *fe;
  struct thread *cur = thread_current();
  struct list_elem *e;
  
  for (e = list_begin(&cur->file_list); e != list_end(&cur->file_list); e = list_next(e))
  {
    fe = list_entry (e, struct flist_pack, elem);
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
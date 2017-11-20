#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <string.h>
#include "threads/synch.h"
#include "vm/page.h"
#include "devices/input.h"

struct lock filesys_lock;
struct lock load_lock;
static int fd_next = 3;

static void syscall_handler (struct intr_frame *); //don't move this to header


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
  lock_init(&load_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *p = f->esp;
  //address safty check with is_user_vaddr() and pagedir_get_page()
  check_addr_safe(p,0,NULL);

  int sysno = *p;
  
  switch(sysno)
  {
    case SYS_HALT:                   /* Halt the operating system. */
    {
      shutdown_power_off();
      NOT_REACHED();
      break;
    }
      

    case SYS_EXIT:                   /* Terminate this process. */
    {
      check_addr_safe(p+1,0,NULL);
      int status = *(int *)(p+1);
      //we must close all files opened by process! LATER
      //OR we must interact with parents MAYBE
      sys_exit(status);
      break;
    }


    case SYS_EXEC:                   /* Start another process. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe((void *)*(p+1),0,NULL);
      char *cmd_line = *(char **)(p+1);
      int tid;

      char *file_name = malloc(strlen(cmd_line)+1); //pure file name to put in filesys_open
      strlcpy(file_name,cmd_line,strlen(cmd_line)+1);
      char *save_ptr;
      file_name = strtok_r(file_name," ",&save_ptr);
      struct file *fp = filesys_open(file_name);
      if(fp == NULL)
      {
        f->eax = TID_ERROR;
      }
      else
      {
        file_close(fp);
        tid = process_execute(cmd_line);
        if(tid == TID_ERROR)
        {
          f->eax = TID_ERROR;
          break;
        }
        struct thread * child = NULL;
        struct thread * t;
        struct list_elem * e;
        for (e = list_begin (&thread_current()->child_list); e != list_end (&thread_current()->child_list);
        e = list_next (e))
        {
          t = list_entry (e, struct thread, child_elem);
          if(t->tid == tid)
          {
            child = t;
            break;
          }
        }
        ASSERT(child);
        sema_down(&child->load);
        f->eax = tid;
        if(child->load_succeed == false)
        {
          f->eax = TID_ERROR;
        }
        
      }
      free(file_name);
      break;
    }
      
    case SYS_WAIT:                   /* Wait for a child process to die. */
    {
      check_addr_safe(p+1,0,NULL);
      int pid = *(int *)(p+1);
      f->eax = process_wait(pid);
      break;
    }
      
    case SYS_CREATE:                 /* Create a file. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe((void *)*(p+1),0,NULL);
      check_addr_safe(p+2,0,NULL);
      char *file = *(char **)(p+1);
      unsigned initial_size = *(unsigned *)(p+2);

      lock_acquire(&filesys_lock);
      f->eax = filesys_create(file, initial_size);
      lock_release(&filesys_lock);
      break;
      
    }

    case SYS_REMOVE:                 /* Delete a file. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe((void *)*(p+1),0,NULL);
      char *file = *(char **)(p+1);

      lock_acquire(&filesys_lock);
      f->eax = filesys_remove(file);
      lock_release(&filesys_lock);
      break;
    }

    case SYS_OPEN:                   /* Open a file. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe((void *)*(p+1),0,NULL);
      char * file_name = *(char **)(p+1);

      lock_acquire(&filesys_lock);
      struct file* fp = filesys_open (file_name);
      
      if(!fp){
        f->eax = -1;
      }
      else{
        struct flist_pack *fe = (struct flist_pack*)malloc(sizeof(struct flist_pack));
        fe->fp = fp;
        fe->fd = fd_next++;
        list_push_back(&thread_current()->file_list, &fe->elem);
        f->eax = fe->fd;
      }
      lock_release(&filesys_lock);
      break;
    }
      

    case SYS_FILESIZE:               /* Obtain a file's size. */
    {
      check_addr_safe(p+1,0,NULL);
      int fd = *(int *)(p+1);
      f->eax = (uint32_t)file_length(fd_to_flist_pack(fd)->fp);
      break;
    }
      
    case SYS_READ:                   /* Read from a file. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe(p+2,0,NULL);
      //check_addr_safe((void *)*(p+2),0,NULL);
      check_addr_safe(p+3,0,NULL);
      int fd = *(int *)(p+1);
      void *buffer = *(void **)(p+2);
      unsigned size = *(unsigned *)(p+3);


      void *test = buffer; //read_boundary
      int sizetest = size;
      while(sizetest--)
      {
        if(!check_addr_safe(test,1,NULL))
          sys_exit(-1);
        struct sp_table_pack * sptp =  upage_to_sp_table_pack(test);
        if(!sptp)
        {
          if(!check_addr_safe(test,2,f->esp))
            sys_exit(-1);
          grow_stack(test);
        }
        else
        {
          if(!sptp->writable)  //only needed in sys read
            sys_exit(-1);
        }
        test++;
        
      }

      if (fd == 0)
      {
        unsigned i;
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
          f->eax = -1;
        }          
        else{
          f->eax = file_read(fe->fp, buffer, (off_t)size);
        }
        lock_release(&filesys_lock);
      }
      
      break;
    }

    case SYS_WRITE:                  /* Write to a file. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe(p+2,0,NULL);
      check_addr_safe((void *)*(p+2),0,NULL);
      check_addr_safe(p+3,0,NULL);
      int fd = *(int *)(p+1);
      void * buffer = *(void **)(p+2);
      unsigned size = *(unsigned *)(p+3);


      void *test = buffer;
      int sizetest = size;
      while(sizetest--)
      {
        if(!check_addr_safe(test,1,NULL))
          sys_exit(-1);
        struct sp_table_pack * sptp =  upage_to_sp_table_pack(test);
        if(!sptp)
        {
          if(!check_addr_safe(test,2,f->esp))
            sys_exit(-1);
          grow_stack(test);
        }
        test++;
        
      }

      if(fd == 1){
        putbuf((char *)buffer,(size_t)size); //too big size may be a problem, but i wont care for now. LATER
        f->eax = size;
      }
      else{
        struct flist_pack *fe = fd_to_flist_pack(fd);
        if (!fe){
          f->eax = -1;
        }          
        else{
          lock_acquire(&filesys_lock);
          f->eax = file_write(fe->fp, buffer, (off_t)size);
          lock_release(&filesys_lock);
        }
      }
      break;
    }


    case SYS_SEEK:                   /* Change position in a file. */
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe(p+2,0,NULL);
      int fd = *(int *)(p+1);
      unsigned position = *(unsigned *)(p+2);

      struct flist_pack *fe = fd_to_flist_pack(fd);
      if (!fe){
        f->eax = -1;
      }
      else{
        lock_acquire(&filesys_lock);
        file_seek(fe->fp, (off_t)position);
        lock_release(&filesys_lock);
      }
      break;
    }

    case SYS_TELL:                   /* Report current position in a file. */
    {
      check_addr_safe(p+1,0,NULL);
      int fd = *(int *)(p+1);

      struct flist_pack *fe = fd_to_flist_pack(fd);

      if (!fe){
        f->eax = -1;
      }
      else{
        lock_acquire(&filesys_lock);
        f->eax = file_tell(fe->fp);
        lock_release(&filesys_lock);
      }
      break;
    }
      
    case SYS_CLOSE:                  /* Close a file. */
    {
      check_addr_safe(p+1,0,NULL);
      int fd = *(int *)(p+1);

      struct list_elem *e;
      struct sp_table_pack *sptp;
      struct thread *cur = thread_current();
      struct flist_pack *fe = fd_to_flist_pack(fd);

      if(!fe)
      {
        sys_exit(-1);
      }
      else
      {
        for(e = list_begin(&cur->sp_table); e != list_end(&cur->sp_table); e = list_next(e))
        {
          sptp = list_entry(e, struct sp_table_pack, elem);
          if(sptp->file == fe->fp && !sptp->is_loaded){
            if(!load_mmap(sptp))
              sys_exit(-11);
          }
        }
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
    {
      check_addr_safe(p+1,0,NULL);
      check_addr_safe(p+2,0,NULL);
      check_addr_safe((void *)*(p+2),0,NULL);
      int fd = *(int *)(p+1);
      void *buffer = *(void **)(p+2);
      
      void *upage = buffer;
      struct thread *cur = thread_current();
      off_t flen, ofs = 0;
      unsigned page_num, i;
      int map_id;
      struct flist_pack *fe = fd_to_flist_pack(fd);
      bool escape = false;
      //printf("----MMAP START----\n");
      if (!fe){
        //printf("----BAD_FD... Process exit----\n");
        sys_exit(-1);
        break;
      }
     // printf("DBG 01\n");

      flen = file_length (fe->fp);
      if (flen <= 0){
        f->eax = -1;
        //printf("----MMAP END----\n");
        break;
      }
      //printf("DBG 02\n");

      page_num = flen / PGSIZE + 1;

      for(i = 0; i < page_num; i++)
      {
        if(upage_to_sp_table_pack(upage + PGSIZE*i)) {
          f->eax = -1;
          escape = true;
          break;
        }
      }
      if(escape){
        //printf("----MMAP END----\n");
        break;
      }
      //printf("DBG 03\n");

      map_id = ++(cur->map_id);

      while (flen > 0)
      {
        size_t page_read_bytes = flen < PGSIZE ? flen : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        struct mmap_file_pack * mmfp;
        struct sp_table_pack *sptp = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
        if (!sptp) {
          cur->map_id--;
          f->eax = -1;
          escape = true;
          break;
        }
        //printf("DBG 04-1\n");

        sptp->owner = cur;
        sptp->upage = upage;
        sptp->is_loaded = false;

        sptp->file = fe->fp;
        sptp->offset = ofs;
        sptp->page_read_bytes = page_read_bytes;
        sptp->page_zero_bytes = page_zero_bytes;
        sptp->writable = true;

        sptp->type = PAGE_MMAP;

        lock_acquire(&cur->sp_table_lock);
        list_push_back(&cur->sp_table, &sptp->elem);
        lock_release(&cur->sp_table_lock);

        mmfp = (struct mmap_file_pack *) malloc(sizeof(struct mmap_file_pack));
        if(!mmfp){
          list_remove(&sptp->elem);
          free(sptp);
          cur->map_id--;
          f->eax = -1;
          escape = true;
          break;
        }
        //printf("DBG 04-2\n");

        mmfp->sptp = sptp;
        mmfp->map_id = cur->map_id;
        lock_acquire(&cur->mmap_lock);
        list_push_back(&cur->mmap_file_list, &mmfp->elem);
        lock_release(&cur->mmap_lock);

        flen -= page_read_bytes;
        upage += PGSIZE;
        ofs += PGSIZE;
      }
      if(escape){
        //printf("----MMAP END----\n");
        break;
      }

      //printf("DBG 05\n");
      f->eax = map_id;
      //printf("----MMAP END----\nMAP ID : %d\n", map_id);
      break;
    }

    case SYS_MUNMAP:                 /* Remove a memory mapping. */
    {
      check_addr_safe(p+1,0,NULL);
      int map_id = *(int *)(p+1);
      sys_munmap(map_id);
      break;
    }

    /* Project 4 only. */
    case SYS_CHDIR:                  /* Change the current directory. */
    {
      break;
    }
    case SYS_MKDIR:                  /* Create a directory. */
    {
      break;
    }
    case SYS_READDIR:                /* Reads a directory entry. */
    {
      break;
    }
    case SYS_ISDIR:                  /* Tests if a fd represents a directory. */
    {
      break;
    }
    case SYS_INUMBER:                 /* Returns the inode number for a fd. */
    {
      break;
    }
    
    default:
      break;
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

void sys_munmap(int map_id)
{
  struct mmap_file_pack *mmfp;
  struct sp_table_pack *sptp = NULL;
  struct thread *cur = thread_current();
  struct list_elem *e;

  if(map_id <= 0){
    return;
  }

  for(e = list_begin(&cur->mmap_file_list); e != list_end(&cur->mmap_file_list); e = list_next(e))
  {
    mmfp = list_entry(e, struct mmap_file_pack, elem);
    if(mmfp->map_id == map_id){
      sptp = mmfp->sptp;
      break;
    }
  }
  if(!sptp){
    return;
  }
  lock_acquire(&filesys_lock);
  file_seek(sptp->file, 0);
  lock_release(&filesys_lock);

  if (pagedir_is_dirty(cur->pagedir, sptp->upage)){
    lock_acquire(&filesys_lock);
    file_write_at(sptp->file, sptp->upage, sptp->page_read_bytes, sptp->offset);
    lock_release(&filesys_lock);
  }

  lock_acquire(&cur->sp_table_lock);
  list_remove(&sptp->elem);
  lock_release(&cur->sp_table_lock);
  free(sptp);
  lock_acquire(&cur->mmap_lock);
  list_remove(&mmfp->elem);
  lock_release(&cur->mmap_lock);
  free(mmfp);
}
#include "vm/page.h"
#include <stdio.h>
#include <debug.h>
#include <string.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "vm/frame.h"

/*
struct sp_table_pack //sup page table
{
    struct thread * owner;
    void *upage;
    bool is_loaded;

    struct file * file;
    off_t offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    enum page_type type;

    struct list_elem elem;
};
*/


bool add_file_to_spage_table(void *upage,struct file * file, off_t offset, size_t page_read_bytes, size_t page_zero_bytes, bool writable)
{
   struct thread *cur = thread_current();
   struct sp_table_pack *spt;
   spt = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
   if(spt == NULL)
    return false;

   spt->owner = cur;
   spt->upage = pg_round_down(upage);
   spt->is_loaded = false;

   spt->file = file;
   spt->offset = offset;
   spt->page_read_bytes = page_read_bytes;
   spt->page_zero_bytes = page_zero_bytes;
   spt->writable = writable;
   spt->type = PAGE_FILE;

   lock_acquire(&cur->sp_table_lock);
   list_push_back(&cur->sp_table, &spt->elem);
   lock_release(&cur->sp_table_lock);
   return true;

}

void free_spage_table(void *upage) // page is uv_addr
{
    struct sp_table_pack *sp_table;
    struct thread *cur = thread_current();
    //maybe we should start lock on this line
    sp_table = upage_to_sp_table_pack(upage);
    ASSERT(sp_table);

    lock_acquire(&cur->sp_table_lock);
    list_remove(&sp_table->elem);
    lock_release(&cur->sp_table_lock);

    palloc_free_page(upage);
    free(sp_table);
}


struct sp_table_pack * upage_to_sp_table_pack(void * upage)
{
    struct list_elem *e;
    struct sp_table_pack *sptp;
    struct thread *cur = thread_current();
    
    upage = pg_round_down(upage); //to know what page the address is in!

    for(e = list_begin(&cur->sp_table); e != list_end(&cur->sp_table); e = list_next(e))
    {
        sptp = list_entry(e, struct sp_table_pack, elem);
        if (sptp->upage == upage){
            return sptp;
        }
    }

    return NULL;
}

bool load_file(struct sp_table_pack * sptp)
{
  ASSERT(sptp->is_loaded == false);

  /* Get a page of memory. */
  uint8_t *kpage = alloc_page_frame (PAL_USER);
  if (kpage == NULL)
    return false;

  /* Load this page. */
  if (file_read_at (sptp->file, kpage, sptp->page_read_bytes,sptp->offset) != (int) sptp->page_read_bytes)
    {
      free_page_frame (kpage);
      return false;
    }
  memset (kpage + sptp->page_read_bytes, 0, sptp->page_zero_bytes);

  /* Add the page to the process's address space. */
  if (!install_page (sptp->upage, kpage, sptp->writable))
    {
      free_page_frame (kpage);
      return false;
    }


    sptp->is_loaded = true;
    return true;
}

bool grow_stack (void *upage)
{
  struct sp_table_pack *sptp;
  struct ftable_pack *ftp;
  struct thread *cur = thread_current();
  //Should Check MAX STACK SIZE


  sptp = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
  if (!sptp){
      return false;
  }
  //Maybe sptp need more init
  sptp->upage = pg_round_down(upage);
  sptp->is_loaded = true;
  sptp->writable = true;
  sptp->type = PAGE_NULL;

  ftp = alloc_page_frame(PAL_USER);
  if (!ftp){
    free(sptp);
    return false;
  }

  if(pagedir_get_page (cur->pagedir, sptp->upage) || !pagedir_set_page (cur->pagedir, sptp->upage, ftp, sptp->writable)){
    free(sptp);
    free(ftp);
    return false;
  }

  lock_acquire(&cur->sp_table_lock);
  list_push_back(&cur->sp_table, &sptp->elem);
  lock_release(&cur->sp_table_lock);

  return true;
}

bool check_addr_safe(const void *vaddr,int mode) //moved this from syscall.c to page.c
{
  if(mode==0)
  {
    if (!vaddr || !is_user_vaddr(vaddr) || !pagedir_get_page(thread_current()->pagedir, vaddr))
      sys_exit(-1);
    }
    
  if(mode==1) //page_fault()
  {//do we have to check user vaddr bottom too?
    if (!vaddr || !is_user_vaddr(vaddr))
    {
      return false;
    }
    else 
      return true;
    }
  if(mode==2) //sys_read ?? 
  {
    if (!vaddr || !is_user_vaddr(vaddr) || !upage_to_sp_table_pack(vaddr))
    {
      sys_exit(-1);
    }
  }
  
}

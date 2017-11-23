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
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

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
   struct sp_table_pack *sptp;
   sptp = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
   if(sptp == NULL)
    return false;

   sptp->owner = cur;
   sptp->upage = pg_round_down(upage);
   sptp->is_loaded = false;

   sptp->file = file;
   sptp->offset = offset;
   sptp->page_read_bytes = page_read_bytes;
   sptp->page_zero_bytes = page_zero_bytes;
   sptp->writable = writable;
   sptp->type = PAGE_FILE;

   lock_acquire(&cur->sp_table_lock);
   list_push_back(&cur->sp_table, &sptp->elem);
   lock_release(&cur->sp_table_lock);
   return true;

}

bool add_mmap_to_spage_table(void *upage,struct file * file, off_t offset, size_t page_read_bytes, size_t page_zero_bytes)
{
  struct thread * cur = thread_current();
  struct mmap_file_pack * mmfp;
  struct sp_table_pack *sptp = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
  if (!sptp) 
  {
    return false;
  }

  sptp->owner = cur;
  sptp->upage = pg_round_down(upage);
  sptp->is_loaded = false;

  sptp->file = file;
  sptp->offset = offset;
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
    return false;
  }

  mmfp->sptp = sptp;
  mmfp->map_id = cur->map_id;

  lock_acquire(&cur->mmap_lock);
  list_push_back(&cur->mmap_file_list, &mmfp->elem);
  lock_release(&cur->mmap_lock);

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

struct sp_table_pack * ftp_to_sptp(struct ftable_pack * ftp)
{
    struct list_elem *e;
    struct sp_table_pack *sptp;
    struct thread * cur = thread_current();

    for(e = list_begin(&cur->sp_table); e != list_end(&cur->sp_table); e = list_next(e))
    {
        sptp = list_entry(e, struct sp_table_pack, elem);
        void * kpage = pagedir_get_page(cur->pagedir,sptp->upage);
        if(kpage == ftp->kpage)
        {
            return sptp;
        }
    }

    return NULL;
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

bool load_mmap(struct sp_table_pack * sptp)
{
  ASSERT(sptp->is_loaded == false);

  /* Get a page of memory. */
  uint8_t *kpage = alloc_page_frame (PAL_USER|PAL_ZERO);
  if (kpage == NULL){
    return false;
  }

  /* Load this page.*/
  if (sptp->page_read_bytes > 0){
    if (file_read_at (sptp->file, kpage, sptp->page_read_bytes, sptp->offset) != (int) sptp->page_read_bytes){
      free_page_frame(kpage);
      return false;
    }
    memset (kpage + sptp->page_read_bytes, 0, sptp->page_zero_bytes);
  }

  /* Add the page to the process's address space. */
  if (!install_page (sptp->upage, kpage, sptp->writable)){
      free_page_frame(kpage);
      return false;
  }

  sptp->is_loaded = true;
  return true;
}

bool load_swap(struct sp_table_pack * sptp)
{
  ASSERT(sptp->is_loaded == false);

  /* Get a page of memory. */
  uint8_t *kpage = alloc_page_frame (PAL_USER|PAL_ZERO);
  if (kpage == NULL){
    return false;
  }
  ASSERT (pg_ofs (sptp->upage) == 0);
  bool success = swap_in(sptp->index, sptp->upage);
  if (success == false){
    return false;
  }

  /* Add the page to the process's address space. */
  if (!install_page (sptp->upage, kpage, true)){
      free_page_frame(kpage);
      return false;
  }

  sptp->is_loaded = true;
  sptp->type = PAGE_FILE;
  return true;
}

bool grow_stack (void *upage) //allocate new frame, get kpage, link upage-kpage
{
  struct sp_table_pack *sptp;
  void *kpage;
  struct thread *cur = thread_current();


  sptp = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
  if (!sptp){
      return false;
  }
  //Maybe sptp need more init
  sptp->upage = pg_round_down(upage);
  sptp->is_loaded = true;
  sptp->writable = true;
  sptp->type = PAGE_NULL;

  kpage = alloc_page_frame(PAL_USER);
  if (!kpage){
    free(sptp);
    return false;
  }

  if(!install_page( sptp->upage, kpage, sptp->writable)){
    free(sptp);
    free_page_frame(kpage);
    return false;
  }

  lock_acquire(&cur->sp_table_lock);
  list_push_back(&cur->sp_table, &sptp->elem);
  lock_release(&cur->sp_table_lock);

  return true;
}

bool check_addr_safe(const void *vaddr,int mode, void * esp)
{
  if(mode==0) // expecting it is part of page, used widely
  {
    if (!vaddr || !is_user_vaddr(vaddr) || !pagedir_get_page(thread_current()->pagedir, vaddr))
      sys_exit(-1);
    else
      return true;
  }

  if(mode==1) // lazy loading is ok, used in page_fault(), virtual address minimal checking
  {
    if (!vaddr || !is_user_vaddr(vaddr) || vaddr < (void *)0x08048000 )
      return false;
    else
      return true;
  }

  if(mode==2) //verify stack , stack heruistic access, stack size 8MB, stack addr upper than code seg
  {
    if (!vaddr || !is_user_vaddr(vaddr)  || vaddr < (void *)0x08048000  || vaddr < esp - 32 || vaddr < PHYS_BASE - 8*1024*1024)
      return false;
    else
      return true;
  }
  if(mode==3) //mmap should NOT be in stack segment
  {
    if (!vaddr || !is_user_vaddr(vaddr)  || vaddr < (void *)0x08048000  || vaddr > PHYS_BASE - 8*1024*1024)
      return false;
    else
      return true;
    }
  NOT_REACHED();
}

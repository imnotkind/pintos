#include "vm/page.h"
#include <stdio.h>
#include <debug.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"


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

void add_to_spage_table(void *upage, bool is_loaded)
{
   struct thread *cur = thread_current(); 
   struct sp_table_pack *spt;
   spt = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
   spt->owner = cur;
   spt->upage = upage;
   spt->is_loaded = is_loaded;
   
   spt->file = NULL;
   spt->offset = 0;
   spt->read_bytes = 0;
   spt->zero_bytes = 0;
   spt->writable = false;
   spt->type = PAGE_NULL;

   lock_acquire(&cur->sp_table_lock);
   list_push_back(&cur->sp_table, &spt->elem);
   lock_release(&cur->sp_table_lock);
}

void add_file_to_spage_table(void *upage, bool is_loaded, struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
   struct thread *cur = thread_current(); 
   struct sp_table_pack *spt;
   spt = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
   spt->owner = cur;
   spt->upage = upage;
   spt->is_loaded = is_loaded;
   
   spt->file = file;
   spt->offset = offset;
   spt->read_bytes = read_bytes;
   spt->zero_bytes = zero_bytes;
   spt->writable = writable;
   spt->type = PAGE_FILE;

   lock_acquire(&cur->sp_table_lock);
   list_push_back(&cur->sp_table, &spt->elem);
   lock_release(&cur->sp_table_lock);

}

void free_page_frame(void *upage) // page is kv_adrr
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
    struct sp_table_pack *spt;
    struct thread *cur = thread_current(); 
    for(e = list_begin(&cur->sp_table); e != list_end(&cur->sp_table); e = list_next(e))
    {
        spt = list_entry(e, struct sp_table_pack, elem);
        if (spt->upage == upage){
            return spt;
        }
    }
    
    return NULL;
}
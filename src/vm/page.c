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
    size_t length;
    bool writable;

    
    enum page_type type;

    struct list_elem elem;
};
*/

void init_spage_table()
{
    list_init(&sp_list);
    lock_init(&sp_table_lock);
}


void add_to_spage_table(void *upage, bool is_loaded)
{
   struct sp_table_pack *spt;
   spt = (struct sp_table_pack *) malloc(sizeof(struct sp_table_pack));
   spt->owner = thread_current();
   spt->upage = upage;
   spt->is_loaded = is_loaded;
   
   spt->file = NULL;
   spt->offset = 0;
   spt->length = 0;
   spt->writable = false;

    
    enum page_type type;
   
   lock_acquire(&sp_table_lock);
   list_push_back(&spt->elem);
   lock_release(&sp_table_lock);
}

void free_page_frame(void *upage) // page is kv_adrr
{
    struct sp_table_pack *sp_table;

    sp_table = upage_to_sp_table_pack(upage);
    ASSERT(sp_table);

    lock_acquire(&sp_table_lock);
    list_remove(&sp_table->elem);
    lock_release(&sp_table_lock);
        
    palloc_free_page(upage);
    free(sp_table);
}

struct sp_table_pack * upage_to_sp_table_pack(void * upage)
{
    struct list_elem *e;
    struct sp_table_pack *spt;
    for(e = list_begin(&sp_list); e != list_end(&sp_list); e = list_next(e))
    {
        spt = list_entry(e, struct sp_table_pack, elem);
        if (f->kpage == kpage){
            return spt;
        }
    }
    
    return NULL;
}
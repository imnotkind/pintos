#include "vm/frame.h"
#include <stdio.h>
#include <debug.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include <random.h>

struct list frame_table;
struct lock eviction_lock;
struct lock ftable_lock;
/*
struct ftable_pack
{
    struct thread* owner;    //owner thread
    void *kpage;    //kernel vaddr
    void *phy;      //physical addr
    bool can_alloc; //availability of allocation
    struct list_elem elem;
};
*/

void init_frame_table()
{
    list_init(&frame_table);
    lock_init(&eviction_lock);
    lock_init(&ftable_lock);
}


void *alloc_page_frame(enum palloc_flags flags)
{
    void *kpage = NULL;
    struct ftable_pack *ftp = NULL;

    ASSERT(flags & PAL_USER);

    kpage = palloc_get_page(flags);
    if(kpage != NULL)
    {
        ftp = (struct ftable_pack *) malloc(sizeof(struct ftable_pack));
        ftp->owner = thread_current();
        ftp->kpage = kpage;
        ftp->phy = (void *) vtop(kpage);
        ftp->can_alloc = false;
        lock_acquire(&ftable_lock);
        list_push_back(&frame_table, &ftp->elem);
        lock_release(&ftable_lock);

        return kpage;
    }
    else //eviction!!
    {
        bool success = evict_frame();
        ASSERT(success);
    
        kpage = palloc_get_page(flags);
        ftp = (struct ftable_pack *) malloc(sizeof(struct ftable_pack));
        ftp->owner = thread_current();
        ftp->kpage = kpage;
        ftp->phy = (void *) vtop(kpage);
        ftp->can_alloc = false;
        lock_acquire(&ftable_lock);
        list_push_back(&frame_table, &ftp->elem);
        lock_release(&ftable_lock);
    }

    
}

void free_page_frame(void *kpage) // page is kv_adrr
{
    struct list_elem *e;
    struct ftable_pack *f;
    struct ftable_pack *ftp = NULL;
    for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
    {
        f = list_entry(e, struct ftable_pack, elem);
        if (f->kpage == kpage){
            ftp = f;
            break;
        }
    }
    ASSERT(ftp);

    lock_acquire(&ftable_lock);
    list_remove(&ftp->elem);
    lock_release(&ftable_lock);
        
    palloc_free_page(kpage);
    free(ftp);
}

struct ftable_pack * kpage_to_ftp(void * kpage)
{
    struct list_elem *e;
    struct ftable_pack *ftp;

    kpage = pg_round_down(kpage); //to know what page the address is in!

    for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
    {
        ftp = list_entry(e, struct ftable_pack, elem);
        if (ftp->kpage == kpage){
            return ftp;
        }
    }

    return NULL;
}


bool evict_frame()
{
    struct ftable_pack * ftp;
    struct sp_table_pack * sptp;

    lock_acquire(&eviction_lock);

    while(1)
    {
        ftp = find_evict_frame(1);
        sptp = ftp_to_sptp(ftp);
        ASSERT(ftp != NULL);
        ASSERT(sptp != NULL);

        if(sptp->pinned == true)
            continue;
        else
            break;
    }
    
    sptp->pinned = true;
    struct thread * owner = sptp->owner;
    
    int index = swap_out(sptp->upage);
    pagedir_clear_page(owner->pagedir,sptp->upage);

    list_remove(&ftp->elem);
    palloc_free_page(ftp->kpage);

    sptp->is_loaded = false;
    sptp->index = index;
    sptp->type = PAGE_SWAP;
    sptp->pinned = false;
    lock_release(&eviction_lock);
    return true;
}

struct ftable_pack * find_evict_frame(int mode)
{
    if(mode==1)//random
    {
        struct list_elem * e;
        struct ftable_pack *victim_frame;
        int list_len = list_size(&frame_table);
        int rand_no = random_ulong() % list_len;

        if(rand_no < 0){
            rand_no = -rand_no;
        }

        for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
            if(rand_no-- == 0){
                victim_frame = list_entry(e, struct ftable_pack, elem);
                return victim_frame;
            }
        }
        NOT_REACHED();
    }
    if(mode==2)//lru
    {

    }

}

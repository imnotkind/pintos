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
    void *kpage;    //kernel vaddr
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
    if(!kpage){
        bool success = evict_frame();
        ASSERT(success);
        kpage = palloc_get_page(flags);
        ASSERT(kpage);
    }

    ftp = (struct ftable_pack *) malloc(sizeof(struct ftable_pack));
    ftp->kpage = kpage;
    lock_acquire(&ftable_lock);
    list_push_back(&frame_table, &ftp->elem);
    lock_release(&ftable_lock);

    return kpage;
}

void free_page_frame(void *kpage) // page is kv_adrr
{
    struct ftable_pack *ftp = NULL;
    ASSERT(pg_ofs(kpage)==0);
    lock_acquire(&ftable_lock);
    
    ftp = kpage_to_ftp(kpage);
    if(!ftp){
        lock_release(&ftable_lock);
        return;
    }
    
    list_remove(&ftp->elem);        
    palloc_free_page(ftp->kpage);
    free(ftp);
    lock_release(&ftable_lock);
}

struct ftable_pack *kpage_to_ftp(void *kpage)
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
        ftp = find_evict_frame(2);
        sptp = ftp_to_sptp(ftp);
        ASSERT(ftp != NULL);
        ASSERT(sptp != NULL);

        if(sptp->pinned == true) //|| ftp->kpage == pagedir_get_page(thread_current()->pagedir,sptp->upage))
            continue;
        else
            break;
    }
    
    sptp->pinned = true;
    struct thread * owner = sptp->owner;
    
    int swap_index = swap_out(sptp->upage);
    ASSERT(swap_index != -1);
    pagedir_clear_page(owner->pagedir,sptp->upage);

    list_remove(&ftp->elem);
    palloc_free_page(ftp->kpage);

    sptp->is_loaded = false;
    sptp->swap_index = swap_index;
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
        struct ftable_pack *ftp;
        unsigned long rand_no = random_ulong() % (unsigned long)list_size(&frame_table);

        for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
            if(rand_no-- == 0){
                ftp = list_entry(e, struct ftable_pack, elem);
                return ftp;
            }
        }
        NOT_REACHED();
    }
    if(mode==2)//clock
    {
        struct list_elem * e ;
        struct ftable_pack *ftp;;
        struct sp_table_pack *sptp;

        for(e = clock_pos; e != list_end(&frame_table); e = list_next(e)){
            ftp = list_entry(e, struct ftable_pack, elem);
            sptp = ftp_to_sptp(ftp);
            if(sptp->pinned == false){
                clock_pos = list_next(e);
                return ftp;
            }
        }
        for(e = list_begin(&frame_table); e != clock_pos;e=list_next(e)){
            ftp = list_entry(e, struct ftable_pack, elem);
            sptp = ftp_to_sptp(ftp);
            if(sptp->pinned == false){
                clock_pos = list_next(e);
                return ftp;
            }
        }
    }

}

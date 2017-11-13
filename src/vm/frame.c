#include "vm/frame.h"
#include <stdio.h>
#include <debug.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

struct list frame_list;
struct lock eviction_lock;
struct lock ftable_lock;
/*
struct ftable_pack
{
    struct thread* t;    //thread id
    void *kpage     //kernel vaddr
    void *phy;      //physical addr
    bool can_alloc; //availability of allocation
    struct list_elem elem;
}
*/

void init_frame_table()
{
    list_init(&frame_list);
    lock_init(&eviction_lock);
    lock_init(&ftable_lock);
}



void *alloc_page_frame(enum palloc_flags flags)
{
    void *kpage = NULL;
    struct ftable_pack *frame = NULL;

    ASSERT(flags & PAL_USER);

    kpage = palloc_get_page(flags);
    ASSERT(kpage != NULL);

    frame = (struct ftable_pack *) malloc(sizeof(struct ftable_pack));
    frame->owner = thread_current();
    frame->kpage = kpage;
    frame->phy = vtop(kpage);
    frame->can_alloc = false;
    lock_acquire(&ftable_lock);
    list_push_back(&frame_list, &frame->elem);
    lock_release(&ftable_lock);

    return kpage;
}

void free_page_frame(void *kpage) // page is kv_adrr
{
    struct list_elem *e;
    struct ftable_pack *f;
    struct ftable_pack *frame = NULL;
    for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e))
    {
        f = list_entry(e, struct ftable_pack, elem);
        if (f->kpage == kpage){
            frame = f;
            break;
        }
    }
    ASSERT(frame);

    lock_acquire(&ftable_lock);
    list_remove(&frame->elem);
    lock_release(&ftable_lock);
        
    palloc_free_page(kpage);
    free(frame);
}

/*
void evict_frame()
{
    struct thread *cur = thread_current();
    lock_acquire(eviction_lock);

    lock_release(eviction_lock);
}

void * find_evict_frame()
{

}
*/
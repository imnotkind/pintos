#include "vm/frame.h"
#include <stdio.h>
#include <debug.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"

struct list frame_list;
struct lock eviction_lock;
struct lock ftable_lock;
/*
struct ftable_pack
{
    struct thread* t;    //thread id
    int fno;    //frame number
    void *page
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
    void *page = NULL;
    struct ftable_pack *frame = NULL;

    ASSERT(flags & PAL_USER);

    page = palloc_get_page(flags);
    ASSERT(page != NULL);

    frame = (struct ftable_pack *) malloc(sizeof(struct ftable_pack));
    frame->t = thread_current();
    frame->fno = 0;
    frame->page = page;
    frame->can_alloc = false;
    lock_acquire(&ftable_lock);
    list_push_back(&frame_list, &frame->elem);
    lock_release(&ftable_lock);

    return page;
}

void free_page_frame(void *page) // page is kv_adrr
{
    struct list_elem *e;
    struct ftable_pack *f;
    struct ftable_pack *frame = NULL;
    for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e))
    {
        f = list_entry(e, struct ftable_pack, elem);
        if (f->page == page){
            frame = f;
            break;
        }
    }
    ASSERT(frame);

    lock_acquire(&ftable_lock);
    list_remove(&frame->elem);
    lock_release(&ftable_lock);
        
    palloc_free_page(page);
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
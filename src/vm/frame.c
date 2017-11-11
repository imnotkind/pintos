#include "vm/frame.h"
#include <stdio.h>
#include <debug.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "therads/thread.h"
#include "threads/malloc.h"

struct list frame_list;
struct lock eviction_lock;

/*
struct ftable_pack
{
    int tid;    //thread id
    int fno;    //frame number
    void *page
    unsigned uv_addr; //user virtual adress
    bool can_alloc; //availability of allocation
    struct list_elem elem;
}
*/

void init_frame_table()
{
    list_init(frame_list);
    lock_init(eviction_lock);
}



void *alloc_page_frame(enum palloc_flags flags)
{
    void *page = NULL;
    struct ftable_pack *frame = NULL;

    ASSERT(flags & PAL_USER);

    page = palloc_get_page(flags);
    ASSERT(page != NULL);

    frame = (struct ftable_pack *) malloc(sizeof(struct ftable_pack));
    frmae->tid = thread_current()->tid;
    frame->fno = 0;
    frame->page = page;
    frmae->uv_addr = 0; //pagedir_set_page에서 초기화
    frmae->can_alloc = false;
    list_push_back(&frmae_list, &frmae->elem);

    return page;
}

void free_page_frame(void *kv_addr)
{
    palloc_free_page(kv_addr);
}

void evict_frame()
{
    struct thread *cur = thread_current();
    lock_acquire(eviction_lock);

    lock_release(eviction_lock);
}

void * find_evict_frmae()
{

}
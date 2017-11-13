#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"

struct ftable_pack
{
    struct thread* owner;    //owner thread
    void *kpage;    //kernel vaddr
    void *phy;      //physical addr
    bool can_alloc; //availability of allocation
    struct list_elem elem;
};

void* alloc_page_frame(enum palloc_flags flags);
void free_page_frame(void *kpage);
void init_frame_table();
#endif
#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"

struct ftable_pack
{
    void *kpage;    //kernel vaddr
    struct list_elem elem;
    bool clock_bit;
};

struct list_elem * clock_pos; //pointer for saving where we find eviction victim
bool first_try;

void* alloc_page_frame(enum palloc_flags flags);
void free_page_frame(void *kpage);
struct ftable_pack * kpage_to_ftp(void * kpage);
void init_frame_table();
bool evict_frame();
struct ftable_pack * find_evict_frame(int mode);
#endif
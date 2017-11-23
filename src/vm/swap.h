#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <stdint.h>
#include "vm/page.h"
#include "threads/synch.h"
#include "devices/block.h"

enum swap_type
{
    SWAP_FREE,
    SWAP_USED
};

struct list swap_table;
struct block *swap_block;
struct lock swap_lock;
struct list_elem *lru_pos; //pointer for saving where we find lru. it looks unstable.

struct swap_table_pack
{
    struct sp_table_pack *sptp;
    struct size_t index;
    enum swap_type type;
    bool recent_used;
    
    struct list_elem elem;
};

void init_swap_table();
struct swap_table_pack* find_lru_stp();
struct swap_table_pack* index_to_swap_table_pack(size_t index);
void swap_in(size_t index, void *upage);
size_t swap_out(void *upage);

#endif
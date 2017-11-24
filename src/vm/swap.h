#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <stdint.h>
#include <bitmap.h>
#include "vm/page.h"
#include "threads/synch.h"
#include "devices/block.h"

enum swap_status
{
    FREE,
    USING
};

struct list swap_table;
struct block *swap_block;
struct lock swap_lock;

struct swap_table_pack
{
    int index;
    enum swap_status status;
    
    struct list_elem elem;
};

void init_swap_table();
struct swap_table_pack* index_to_swap_table_pack(int index);
bool swap_in(int index, void *upage);
int swap_out(void *upage);

#endif
#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <stdint.h>
#include "vm/page.h"
#include "threads/synch.h"
#include "devices/block.h"

enum swap_status
{
    IN_BUFFER,
    IN_BLOCK
};

enum stable

struct list swap_table;
struct block *swap_block;
struct lock swap_lock;
struct list_elem *lru_pos; //pointer for saving where we find lru. it looks unstable.

struct swap_table_pack
{
    struct sp_table_pack *sptp;
    int index;
    enum swap_status status;
    
    struct list_elem elem;
};

void init_swap_table();
struct swap_table_pack* find_lru_stp();
struct swap_table_pack* index_to_swap_table_pack(int index);
bool swap_in(int index, void *upage);
int swap_out(void *upage);

#endif
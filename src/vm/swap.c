#include "vm/swap.h"
#include "userprog/syscall.h"
#include <stdio.h>
#include "threads/vaddr.h"

extern struct lock filesys_lock;

void init_swap_table()
{
    swap_block = block_get_role(BLOCK_SWAP);
    if(!swap_block){
        return;
    }

    int i=0;
    for(i; i < block_size(swap_block)*BLOCK_SECTOR_SIZE/PGSIZE; i++)
    {
        struct swap_table_pack * stp = malloc(sizeof(struct swap_table_pack));
    }

    list_init(&swap_table);
    lock_init(&swap_lock);
    lru_pos = list_begin(&swap_table);
}

//check swap table twice.
struct swap_table_pack * find_lru_stp()
{
    struct list_elem *e;
    struct swap_table_pack *stp;
    struct swap_table_pack *lru_stp = NULL;

    //first loop : lru_pos -> end
    for(e = lru_pos; e != list_end(&swap_table); e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->recent_used == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->recent_used = false;
    }
    //second loop : begin -> end
    for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->recent_used == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->recent_used = false;
    }
    //third loop : begin -> lru_pos
    for(e = list_begin(&swap_table); e != lru_pos; e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->recent_used == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->recent_used = false;
    }
    return NULL;
}
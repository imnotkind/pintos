#include "vm/swap.h"
#include <stdio.h>
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

extern struct lock filesys_lock;

void init_swap_table()
{
    int i;
    swap_block = block_get_role(BLOCK_SWAP);
    if(!swap_block){
        return;
    }

    list_init(&swap_table);
    lock_init(&swap_lock);
    
    for(i = 0; i < block_size(swap_block)/8; i++){
        struct swap_table_pack *stp = malloc(sizeof(struct swap_table_pack));
        ASSERT(stp != NULL);
        stp->status = FREE;
        stp->index = i+1;
        list_push_back(&swap_table,&stp->elem);
    }
}

//swap into memory with index, block -> buffer(page)
bool swap_in(int index, void *upage)
{
    struct swap_table_pack *stp;
    int i;

    lock_acquire(&filesys_lock);
	lock_acquire(&swap_lock);
    
    stp = index_to_swap_table_pack(index);
	if (!stp || stp->status == FREE){ 
		lock_release(&swap_lock);
		return false;
	}
    

	for (i = 0; i < 8; i++){
		block_read (swap_block, index * 8 + i, (uint8_t *) upage + i*BLOCK_SECTOR_SIZE);
	}

    stp->status = FREE;
    lock_release(&swap_lock);
    lock_release(&filesys_lock);
    return true;
}

//swap out from memory, buffer(page) -> block
int swap_out(void *upage)
{
    struct list_elem *e;
    struct swap_table_pack *stp;
    int index = -1;
    int i;
	if (swap_block == NULL){
		return -1;
	}

    lock_acquire(&filesys_lock);
    lock_acquire(&swap_lock);


	for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e), index++){
		stp = list_entry(e, struct swap_table_pack, elem);
		if(stp->status == FREE){
			index = stp->index;
			break;
		}
	}

	for (i = 0; i < 8; i++) {
		block_write (swap_block, index * 8 + i, (uint8_t *) upage + i*BLOCK_SECTOR_SIZE);
    }
    stp->status = USING;
    lock_release(&swap_lock);
    lock_release(&filesys_lock);
    return index;
}


//find swap table pack with index. return NULL when can't find
struct swap_table_pack* index_to_swap_table_pack(int index)
{
    struct list_elem *e;
    struct swap_table_pack *spt;
    
	if(list_empty(&swap_table)){
        return NULL;
    }
	
    for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		spt = list_entry(e, struct swap_table_pack, elem);
		if(spt->index == index){
            return spt;
        }
    }
    return NULL;
}

/*
//check swap table twice.
struct swap_table_pack* find_clock_stp()
{
    struct list_elem *e;
    struct swap_table_pack *stp;

    //first loop : lru_pos -> end
    for(e = lru_pos; e != list_end(&swap_table); e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->status == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->status = false;
    }
    //second loop : begin -> end
    for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->status == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->status = false;
    }
    //third loop : begin -> lru_pos
    for(e = list_begin(&swap_table); e != lru_pos; e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->status == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->status = false;
    }
    return NULL;
}
*/
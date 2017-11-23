#include "vm/swap.h"
#include "userprog/syscall.h"
#include <stdio.h>
#include "threads/vaddr.h"

extern struct lock filesys_lock;

void init_swap_table()
{
    int i;
    swap_block = block_get_role(BLOCK_SWAP);
    if(!swap_block){
        return;
    }
    
    for(i = 0; i < block_size(swap_block)* BLOCK_SECTOR_SIZE/PGSIZE; i++){
        struct swap_table_pack * stp = malloc(sizeof(struct swap_table_pack));
        //additional init
    }

    list_init(&swap_table);
    lock_init(&swap_lock);
    lru_pos = list_begin(&swap_table);
}

//swap into memory with index
void swap_in(size_t index, void *upage)
{
    int i;
    struct swap_table_pack *stp;

	lock_acquire(&swap_lock);
	stp = index_to_swap_table_pack(index);
	
	if (stp == NULL || stp->recent_used == true){ //it was 'available' in cctv. we need check
		lock_release(&swap_lock);
		return;
	}

	stp->recent_used = true; //this is same wih upper one.

	for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++){
		block_read (swap_block, index*PGSIZE/BLOCK_SECTOR_SIZE + i, (uint8_t *) upage + i*BLOCK_SECTOR_SIZE);
	}
    lock_release(&swap_lock);
}

//swap out from memory
size_t swap_out(void *upage)
{

}

//find swap table pack with index. return NULL when can't find
struct swap_table_pack* index_to_swap_table_pack(size_t index)
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

//check swap table twice.
struct swap_table_pack* find_lru_stp()
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
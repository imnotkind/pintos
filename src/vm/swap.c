#include "vm/swap.h"
#include <stdio.h>
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

extern struct lock filesys_lock;

void init_swap_table()
{
    unsigned i;
    swap_block = block_get_role(BLOCK_SWAP);
    if(!swap_block){
        return;
    }

    list_init(&swap_table);
    lock_init(&swap_lock);
    lru_pos = list_begin(&swap_table);
    
    for(i = 0; i < block_size(swap_block)* BLOCK_SECTOR_SIZE/PGSIZE; i++){
        struct swap_table_pack *stp = malloc(sizeof(struct swap_table_pack));
        ASSERT(stp != NULL);
        stp->can_alloc =true;
        stp->index = i+1;
        list_push_back(&swap_table,&stp->elem);
    }

    
}

//swap into memory with index
void swap_in(size_t index, void *upage)
{
    int i;
    struct swap_table_pack *stp;

	lock_acquire(&swap_lock);
    
    stp = index_to_swap_table_pack(index);
	if (!stp || stp->can_alloc == true){ 
		lock_release(&swap_lock);
		return;
	}

	stp->can_alloc = true; 

	for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++){
		block_read (swap_block, index*PGSIZE/BLOCK_SECTOR_SIZE + i, (uint8_t *) upage + i*BLOCK_SECTOR_SIZE);
	}
    lock_release(&swap_lock);
}

//swap out from memory
size_t swap_out(void *upage)
{
    struct list_elem *e;
    struct swap_table_pack *stp;
    size_t index = 1;
    int i;
	if (swap_block == NULL || &swap_table == NULL){
		return -1;
	}

	lock_acquire(&swap_lock);

	for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e), index++){
		stp = list_entry(e, struct swap_table_pack, elem);
		if(stp->can_alloc == true){
			stp->can_alloc = false;
			index = stp->index;
			break;
		}
	}

	if(index == list_size(&swap_table)){
		lock_release(&swap_lock);
		return -1;
	}

	for (i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++) {
		block_write (swap_block, index*PGSIZE/BLOCK_SECTOR_SIZE + i, (uint8_t *) upage + i*BLOCK_SECTOR_SIZE);
	}
	lock_release(&swap_lock);
    return index;
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

    //first loop : lru_pos -> end
    for(e = lru_pos; e != list_end(&swap_table); e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->can_alloc == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->can_alloc = false;
    }
    //second loop : begin -> end
    for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->can_alloc == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->can_alloc = false;
    }
    //third loop : begin -> lru_pos
    for(e = list_begin(&swap_table); e != lru_pos; e = list_next(e)){
        stp = list_entry(e, struct swap_table_pack, elem);
        if(stp->can_alloc == false){
            lru_pos = list_next(e);
            return stp;
        }
        stp->can_alloc = false;
    }
    return NULL;
}
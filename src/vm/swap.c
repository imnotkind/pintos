#include "vm/swap.h"
#include <stdio.h>

void init_swap_table()
{
    swap_block = block_get_role(BLOCK_SWAP);
    if(!swap_block){
        return;
    }

    list_init(&swap_list);
    lock_init(&swap_lock);
}


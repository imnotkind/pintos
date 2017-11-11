#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

struct ftable_pack
{
    int tid;    //thread id
    int fno;    //frame number
    void *page;
    unsigned uv_addr; //user virtual adress
    bool can_alloc; //availability of allocation
    struct list_elem elem;
}


#endif
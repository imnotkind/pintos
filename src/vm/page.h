#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>

struct sptable_pack
{
    void *upage;
    bool can_write;
    struct list_elem elem;
};

#endif
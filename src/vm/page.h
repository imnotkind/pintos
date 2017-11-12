#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>

struct sp_table_pack //sup page table
{
    void *upage;
    bool is_loaded;
    struct list_elem elem;
};

#endif
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>

enum page_type
{
    PAGE_FILE
};

struct sp_table_pack //sup page table
{
    struct thread * owner;
    void *upage;
    bool is_loaded;
    
    struct file * file;
    
    enum page_type type;

    struct list_elem elem;
};

#endif
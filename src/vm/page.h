#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include <stdint.h>
#include "filesys/file.h"

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
    off_t offset;
    size_t length;
    bool writable;

    
    enum page_type type;

    struct list_elem elem;
};

void add_to_spage_table(void *upage, bool is_loaded);
void free_page_frame(void *upage);
struct sp_table_pack * upage_to_sp_table_pack(void * upage);

#endif
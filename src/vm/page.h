#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include <stdint.h>
#include "filesys/file.h"

enum page_type
{
    PAGE_NULL,
    PAGE_FILE
};

struct sp_table_pack //sup page table
{
    struct thread * owner;
    void *upage;
    bool is_loaded;

    struct file * file;
    off_t offset;      //file offset
    size_t page_read_bytes;
    size_t page_zero_bytes;
    bool writable;


    enum page_type type;

    struct list_elem elem;
};

struct mmap_file_pack
{
    struct sp_table_pack *sptp;
    int map_id;
    struct list_elem elem;
};

bool add_file_to_spage_table(void *upage, struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void free_spage_table(void *upage);
struct sp_table_pack * upage_to_sp_table_pack(void * upage);
bool check_addr_safe(const void *vaddr,int mode, void * esp);
bool load_file(struct sp_table_pack * sptp);
bool grow_stack (void *upage);
#endif

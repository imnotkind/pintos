#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "threads/synch.h"
#include "devices/block.h"

struct buffer_cache
{
    char buffer[BLOCK_SECTOR_SIZE];
    block_sector_t sector;
    bool is_dirty;
    bool is_using;
    bool recent_used;
    struct lock buffer_lock;
};

void init_buffer_cache();
void clear_cache(struct buffer_cache *bc);
void cache_read(block_sector_t sector, off_t sect_ofs void *buffer, off_t buf_ofs, int read_bytes);
void cache_write(block_sector_t sector, off_t sect_ofs void *buffer, off_t buf_ofs, int write_bytes);
struct buffer_cache * cache_evict();
struct buffer_cache * find_evict_cache();
struct buffer_cache * find_empty_cache();
struct buffer_cache * sector_to_cache(block_sector_t sector);

#endif
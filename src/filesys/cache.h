#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <list.h>
#include "threads/synch.h"
#include "filesys/inode.h"

struct buffer_cache
{
    char buffer[BLOCK_SECTOR_SIZE];
    block_sector_t sector;
    bool is_dirty;
    int is_using;
    bool clock_bit; //clock bit for clock algo
    struct lock buffer_lock;
};

void init_buffer_caches();
void reset_buffer_caches();
void clear_cache(struct buffer_cache *bc);
void cache_read(block_sector_t sector, off_t sect_ofs, void *buffer, off_t buf_ofs, int read_bytes);
void read_ahead(block_sector_t * sector_p);
void cache_write(block_sector_t sector, off_t sect_ofs, void *buffer, off_t buf_ofs, int write_bytes);
void write_back();
struct buffer_cache * cache_evict();
struct buffer_cache * find_evict_cache();
struct buffer_cache * find_empty_cache();
struct buffer_cache * sector_to_cache(block_sector_t sector);

#endif
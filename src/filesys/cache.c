#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include <string.h>
#include <debug.h>
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/malloc.h"

#define BUFFER_CACHE_NUM 64

/*
struct buffer_cache
{
    bool is_dirty;
    bool can_use;
    struct lock buffer_lock;
    block_sector_t sector;
    char buffer[BLOCK_SECTOR_SIZE];
};
*/

struct buffer_cache buffer_cache[BUFFER_CACHE_NUM];
struct lock buffer_cache_lock;
int clock_pos;

void init_buffer_caches()
{
    int i;
    for(i = 0; i < BUFFER_CACHE_NUM; i++){
        clear_cache(&buffer_cache[i]);
        lock_init(&buffer_cache[i].buffer_lock);
    }
    lock_init(&buffer_cache_lock);
    clock_pos = 0;
    thread_create("write back scheduler",0,write_back,NULL);
}

void reset_buffer_caches()
{
    int i;
    for(i = 0; i < BUFFER_CACHE_NUM; i++){
        lock_acquire(&buffer_cache[i].buffer_lock);
        if(buffer_cache[i].is_dirty)
            block_write(fs_device, buffer_cache[i].sector, buffer_cache[i].buffer);
        clear_cache(&buffer_cache[i]);
        lock_release(&buffer_cache[i].buffer_lock);
    }
    clock_pos = 0;
}

void clear_cache(struct buffer_cache *bc)
{
    memset(bc->buffer, 0, BLOCK_SECTOR_SIZE);
    bc->is_dirty = false;
    bc->is_using = false;
    bc->clock_bit = false;
    bc->sector = (block_sector_t) -1;
}

struct buffer_cache * find_evict_cache() //loop all buffer caches twice
{
    int i;
    for(i = clock_pos; i < BUFFER_CACHE_NUM; i++){
        if(buffer_cache[i].clock_bit == false && buffer_cache[i].is_using == true){
            clock_pos = i;
            return &buffer_cache[i];
        }
        buffer_cache[i].clock_bit = false;
    }
    for(i = 0; i < BUFFER_CACHE_NUM; i++){
        if(buffer_cache[i].clock_bit == false && buffer_cache[i].is_using == true){
            clock_pos = i;
            return &buffer_cache[i];
        }
        buffer_cache[i].clock_bit = false;
    }
    for(i = 0; i < clock_pos; i++){
        if(buffer_cache[i].clock_bit == false && buffer_cache[i].is_using == true){
            clock_pos = i;
            return &buffer_cache[i];
        }
        buffer_cache[i].clock_bit = false;
    }
    return NULL;
}

struct buffer_cache * cache_evict()
{
    struct buffer_cache *bc;
    bc = find_evict_cache();
    ASSERT(bc && bc->is_using == true);

    lock_acquire(&bc->buffer_lock);
    if(bc->is_dirty) //WRITE BACK(BEHIND)!!!
        block_write(fs_device, bc->sector, bc->buffer);
    clear_cache(bc);
    lock_release(&bc->buffer_lock);
    return bc;
}

void cache_read(block_sector_t sector, off_t sect_ofs, void *buffer, off_t buf_ofs, int read_bytes) //read from buffer cache instead of disk
{
    struct buffer_cache *bc;
    ASSERT(read_bytes > 0 && read_bytes <= BLOCK_SECTOR_SIZE);

    lock_acquire(&buffer_cache_lock);
    bc = sector_to_cache(sector);
    if(!bc)
    {
        bc = find_empty_cache();
        if(!bc)
            bc = cache_evict();
        block_read(fs_device, sector, bc->buffer); 
        bc->sector = sector;
        bc->is_using = true;
        bc->is_dirty = false;
        block_sector_t * next_sector = (block_sector_t *)malloc(sizeof(uint32_t));
        *next_sector = sector+1;
        thread_create("read ahead",0,read_ahead,next_sector);
    }
    ASSERT(bc->sector != (block_sector_t) -1);
    ASSERT(bc->is_using == true);

    lock_acquire(&bc->buffer_lock);
    lock_release(&buffer_cache_lock);

    memcpy(buffer + buf_ofs, bc->buffer + sect_ofs, read_bytes);
    bc->clock_bit = true;
    lock_release(&bc->buffer_lock);
}

void read_ahead(block_sector_t * sector_p)
{
    struct buffer_cache *bc;
    lock_acquire(&buffer_cache_lock);
    bc = sector_to_cache(*sector_p);
    if(!bc) //not in cache
    {
        bc = find_empty_cache();
        if(!bc)
            bc = cache_evict();
        block_read(fs_device, *sector_p, bc->buffer); 
        bc->sector = *sector_p;
        bc->is_using = true;
        bc->is_dirty = false;
    }
    free(sector_p);
    lock_release(&buffer_cache_lock);
}

void cache_write(block_sector_t sector, off_t sect_ofs, void *buffer, off_t buf_ofs, int write_bytes) //write to buffer cache instead of disk
{
    struct buffer_cache *bc;
    ASSERT(write_bytes > 0 && write_bytes <= BLOCK_SECTOR_SIZE);

    lock_acquire(&buffer_cache_lock);
    bc = sector_to_cache(sector);
    if(!bc){
        bc = find_empty_cache();
        if(!bc)
            bc = cache_evict();
        block_read(fs_device, sector, bc->buffer); 
        bc->sector = sector;
        bc->is_using = true;
        bc->is_dirty = false;
    }
    ASSERT(bc->sector != (block_sector_t) -1);
    ASSERT(bc->is_using == true);

    lock_acquire(&bc->buffer_lock);
    lock_release(&buffer_cache_lock);

    memcpy(bc->buffer + sect_ofs, buffer + buf_ofs, write_bytes);
    bc->clock_bit = true;
    bc->is_dirty = true; //write back
    lock_release(&bc->buffer_lock);
}

void write_back()
{
    int i;
    while(1)
    {
        timer_sleep(1);
        for(i=0;i<BUFFER_CACHE_NUM;i++) //buffer cache -> disk
        {
            lock_acquire(&buffer_cache[i].buffer_lock); 
            if(buffer_cache[i].is_dirty == true)
                block_write(fs_device, buffer_cache[i].sector, buffer_cache[i].buffer);
            buffer_cache[i].is_dirty = false;
            lock_release(&buffer_cache[i].buffer_lock);
        }
    }
    
}

struct buffer_cache * find_empty_cache()
{
    int i;
    for(i = 0; i < BUFFER_CACHE_NUM; i++){
        if(buffer_cache[i].is_using == false){
            return &buffer_cache[i];
        }
    }

    return NULL;
}

struct buffer_cache * sector_to_cache(block_sector_t sector)
{
    int i;
    for(i = 0; i < BUFFER_CACHE_NUM; i++){
        if(buffer_cache[i].sector == sector){
            return &buffer_cache[i];
        }
    }

    return NULL;
}
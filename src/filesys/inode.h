#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <list.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes(= 512 bytes) long. */
struct inode_disk
  {
    block_sector_t direct;              // sector index of direct 512 Bytes
    block_sector_t indirect;            // sector index of indirect 128 * 512 B = 64 KB
    block_sector_t double_indirect;     // sector index of double indirect 128 * 64 KB = 8 MB

    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    int is_dir;                    // We use this like bool type because we need to size inode_disk to 512 byte

    uint32_t unused[122];               /* Not used. */
};


/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. (inode_disk) */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    
    struct lock inode_lock;
  };



struct bitmap;

void inode_init (void);
bool inode_create (block_sector_t sector, off_t length, int is_dir);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_dealloc(struct inode_disk * disk_inode);
bool inode_growth(struct inode_disk* disk_inode, off_t new_length);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
bool inode_is_dir(struct inode *);

#endif /* filesys/inode.h */

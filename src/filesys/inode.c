#include "filesys/inode.h"
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
/* BLOCK_SECTOR_SIZE / sizeof(int32_t) : max number of pointers in one block(sector) */
#define INT32T_PER_SECTOR 128
/* 8 MB */
#define MAX_FILE_LENGTH 8 * 1024 * 1024


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode_disk *idisk, off_t pos) 
{
  block_sector_t tmp_buffer[INT32T_PER_SECTOR];
  block_sector_t ind_sector; //indirect

  ASSERT (idisk != NULL);
  if (pos >= idisk->length)
    return -1;
  if(pos < BLOCK_SECTOR_SIZE){
    //when data is in the direct disk
    return idisk->direct;
  }
  else if(pos < (INT32T_PER_SECTOR + 1) * BLOCK_SECTOR_SIZE){
    //when data is in the indirect disk
    if(idisk->indirect == (block_sector_t) -1){
      return -1;
    }
    cache_read(idisk->indirect, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE); //read the indirect sector of inode_disk(array of pointers)
    pos -= BLOCK_SECTOR_SIZE;
    return tmp_buffer[pos / BLOCK_SECTOR_SIZE];
  }
  else if(pos < (INT32T_PER_SECTOR*INT32T_PER_SECTOR + INT32T_PER_SECTOR + 1) * BLOCK_SECTOR_SIZE){
    //when data is in the double indirect disk
    if(idisk->double_indirect == (block_sector_t) -1){
      return -1;
    }
    cache_read(idisk->double_indirect, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE); //tmp_disk.block is now made of indirect sector
    pos -= (INT32T_PER_SECTOR + 1) * BLOCK_SECTOR_SIZE;
    ind_sector = tmp_buffer[pos / (INT32T_PER_SECTOR * BLOCK_SECTOR_SIZE)]; 

    if(ind_sector == (block_sector_t) -1){
      return -1;
    }
    cache_read(ind_sector, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE);  //tmp_disk.block is now made of direct sector num
    pos %= INT32T_PER_SECTOR * BLOCK_SECTOR_SIZE;
    return tmp_buffer[pos / BLOCK_SECTOR_SIZE];
  }
  else{
    NOT_REACHED(); //8MB file limit
  }
  NOT_REACHED();
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, int is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0 || length < MAX_FILE_LENGTH);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
      disk_inode->direct = (block_sector_t) -1;
      disk_inode->indirect = (block_sector_t) -1;
      disk_inode->double_indirect = (block_sector_t) -1;
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;

      if(!inode_growth(disk_inode,length))
      {
        free(disk_inode);
        return false;
      }
      
      cache_write (sector, 0, disk_inode, 0, BLOCK_SECTOR_SIZE);
      free(disk_inode);
      success = true;

  }
  return success;
}

// add sectors until sector count reach to DIV_ROUND_UP((new_length)/BLOCK_SECTOR_SIZE)
bool inode_growth(struct inode_disk* disk_inode, off_t new_length)
{
  //struct inode_disk * disk_inode = &inode->data;
  int sectors, new_sectors, i;
  int indirect_idx, double_idx; //index in single & double indirect sector table
  block_sector_t sector, tmp_sector;
  static char zeros[BLOCK_SECTOR_SIZE];      //init direct sector to 0
  block_sector_t indirect_init[INT32T_PER_SECTOR]; //init single & double indirect sector to -1
  block_sector_t tmp_buffer[INT32T_PER_SECTOR];
  for(i = 0; i < INT32T_PER_SECTOR; i++){
    indirect_init[i] = (block_sector_t) -1;
  }

  ASSERT(new_length < MAX_FILE_LENGTH);
  if(new_length == disk_inode->length){
    return true;
  }
  else if(new_length < disk_inode->length){
    ASSERT(0);
    return true;
  }
  sectors = DIV_ROUND_UP(disk_inode->length, BLOCK_SECTOR_SIZE);
  new_sectors = DIV_ROUND_UP(new_length, BLOCK_SECTOR_SIZE);

  for(sectors++; sectors <= new_sectors; sectors++)
  {
    ASSERT(sectors > 0);

    if(byte_to_sector (disk_inode, sectors * BLOCK_SECTOR_SIZE - 1) != (block_sector_t) -1){ //grow when sector is not full
      continue;
    }
    if(!free_map_allocate(1, &sector)){ //allocate only one sector(not contiguous!)
      return false;
    }
    
    if(sectors == 1)
    {
      //when direct
      cache_write(sector, 0, zeros, 0, BLOCK_SECTOR_SIZE);
      disk_inode->direct = sector;
    }
    else if(sectors <= INT32T_PER_SECTOR + 1)
    {
      //when indirect
      indirect_idx = sectors - 2; // array index is started with 0

      if(disk_inode->indirect == (block_sector_t) -1){
        disk_inode->indirect = sector;
        cache_write(sector, 0, indirect_init, 0, BLOCK_SECTOR_SIZE);
        sectors--; //this doesn't count to sectors
        continue;
      }
      cache_read(disk_inode->indirect, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE);
      tmp_buffer[indirect_idx] = sector;
      cache_write(disk_inode->indirect, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE);
    }
    else if(sectors <= INT32T_PER_SECTOR*INT32T_PER_SECTOR + INT32T_PER_SECTOR + 1)
    {
      //when double_indirect
      indirect_idx = sectors - (INT32T_PER_SECTOR + 2); // array index is started with 0
      double_idx = indirect_idx / INT32T_PER_SECTOR;
      indirect_idx = indirect_idx % INT32T_PER_SECTOR;

      if(disk_inode->double_indirect == (block_sector_t) -1){
        disk_inode->double_indirect = sector;
        cache_write(sector, 0, indirect_init, 0, BLOCK_SECTOR_SIZE); //double indirect sector init
        sectors--; //this doesn't count to sectors
        continue;
      }

      cache_read(disk_inode->double_indirect, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE);

      if(tmp_buffer[double_idx] == (block_sector_t) -1){
        tmp_buffer[double_idx] = sector;
        cache_write(disk_inode->double_indirect, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE); //add single indirect in double indirect sector
        cache_write(sector, 0, indirect_init, 0, BLOCK_SECTOR_SIZE); //single indirect sector init
        sectors--; //this doesn't count to sectors
        continue;
      }
      
      tmp_sector = tmp_buffer[double_idx];
      cache_read(tmp_sector, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE);
      tmp_buffer[indirect_idx] = sector;
      cache_write(tmp_sector, 0, tmp_buffer, 0, BLOCK_SECTOR_SIZE);

    }
    else
    {
      NOT_REACHED();
    }
    cache_write(sector, 0, zeros, 0, BLOCK_SECTOR_SIZE);
  }

  disk_inode->length = new_length;
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->inode_lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          //free_map_release (inode->data.start,bytes_to_sectors (inode->data.length)); 
          //NEED TO DEALLOCATE BY INDEX!!!
          struct inode_disk idisk;
          cache_read(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE);
          inode_dealloc(&idisk);
        }

      free (inode); 
    }
}

void
inode_dealloc(struct inode_disk * disk_inode)
{
  block_sector_t tmp_buffer[INT32T_PER_SECTOR];
  block_sector_t tmp_indirect_buffer[INT32T_PER_SECTOR];
  int i,j;

  if(disk_inode->direct != -1)
    free_map_release(disk_inode->direct,1);

  if(disk_inode->indirect != -1)
  {
    cache_read(disk_inode->indirect,0,tmp_buffer,0,BLOCK_SECTOR_SIZE);
    for(i=0;i<INT32T_PER_SECTOR;i++)
    {
      if(tmp_buffer[i] != -1)
      {
        free_map_release(tmp_buffer[i],1);
      }
    }
    free_map_release(disk_inode->indirect,1);
  }

  if(disk_inode->double_indirect != -1)
  {
    cache_read(disk_inode->double_indirect,0,tmp_indirect_buffer,0,BLOCK_SECTOR_SIZE);
    for(i=0;i<INT32T_PER_SECTOR;i++)
    {
      if(tmp_indirect_buffer[i] != -1)
      {
        cache_read(tmp_indirect_buffer[i],0,tmp_buffer,0,BLOCK_SECTOR_SIZE);
        for(j=0;j<INT32T_PER_SECTOR;j++)
        {
          if(tmp_buffer[j] != -1)
          {
            free_map_release(tmp_buffer[j],1);
          }
        }
        free_map_release(tmp_indirect_buffer[i],1);
      }
    }
    free_map_release(disk_inode->double_indirect,1);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      struct inode_disk idisk;
      cache_read(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE);
      block_sector_t sector_idx = byte_to_sector (&idisk, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      
      cache_read(sector_idx, sector_ofs, buffer, bytes_read, chunk_size);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  struct inode_disk idisk;
  cache_read(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE);

  if (inode->deny_write_cnt)
    return 0;

  if(idisk.length < offset + size)
  {
    if(idisk.is_dir == 0)
      lock_acquire(&inode->inode_lock);
    if(!inode_growth(&idisk,offset + size))
      NOT_REACHED();
    if(idisk.is_dir == 0)
      lock_release(&inode->inode_lock);
    cache_write(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE); //update changed inode_disk in disk
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      cache_read(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE);
      block_sector_t sector_idx = byte_to_sector (&idisk, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      
      cache_write(sector_idx, sector_ofs, buffer, bytes_written, chunk_size);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk idisk;
  cache_read(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE);
  return idisk.length;
}

bool
inode_is_dir(struct inode *inode)
{
  struct inode_disk idisk;
  if(inode->removed)
    return false;
  cache_read(inode->sector,0,&idisk,0,BLOCK_SECTOR_SIZE);
  if(idisk.is_dir == 0)
    return false;
  else
    return true;
  NOT_REACHED();
}
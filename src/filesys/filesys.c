#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"

#define PATH_MAX 256
#define DIR_ENTRIES 32
/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");
  
  init_buffer_caches();
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  thread_current()->current_dir = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  reset_buffer_caches();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  char name[NAME_MAX + 1]; //to contain \0
  struct dir *dir = parse_path(path, name);
  bool success;

  if(is_dir){ // when it is dir
    success = (dir != NULL
              && free_map_allocate (1, &inode_sector)
              && dir_create (inode_sector, DIR_ENTRIES) //not dir
              && dir_add (dir, name, inode_sector));
  }
  else{ // when it is file
    success = (dir != NULL
              && free_map_allocate (1, &inode_sector)
              && inode_create (inode_sector, initial_size, 0) //not dir
              && dir_add (dir, name, inode_sector));
  }

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{
  char name[NAME_MAX + 1]; //to contain \0
  struct dir *dir = parse_path (path, name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path) 
{
  char name[NAME_MAX + 1], temp[PATH_MAX+1]; //to contain \0
  struct dir *dir, *cur_dir = NULL;
  struct inode *inode;
  bool success = false;
  
  dir = parse_path (path, name);
  dir_lookup(dir, name, &inode);

  cur_dir = dir_open(inode);

  if(!inode_is_dir(inode) || (cur_dir && !dir_readdir(cur_dir, temp)){
    success = dir != NULL && dir_remove (dir, name);
  }
  dir_close (dir); 
  
  if(cur_dir){
    dir_close(cur_dir);
  }

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  struct dir *root;
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  
  root = dir_open_root();
  dir_add(root, ".", ROOT_DIR_SECTOR);
  dir_add(root, "..", ROOT_DIR_SECTOR);
  dir_close(root);

  free_map_close ();
  printf ("done.\n");
}

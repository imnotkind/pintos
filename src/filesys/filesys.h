#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/directory.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */
#define PATH_MAX 256
#define DIR_ENTRIES 16

/* Block device that contains the file system. */
struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);
struct dir * parse_path(char * path, char * name);
int find_first_slash(char * str, int start_pos);
#endif /* filesys/filesys.h */

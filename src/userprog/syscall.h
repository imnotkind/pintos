#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "filesys/file.h"

struct flist_pack
{
  int fd; //file descriptor
  struct file *fp; //file pointer
  struct list_elem elem;
};

void syscall_init (void);
void sys_exit(int status);
void check_addr_safe(const void *vaddr);
struct flist_pack* fd_to_flist_pack(int fd);

#endif /* userprog/syscall.h */

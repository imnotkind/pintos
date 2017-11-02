#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct flist_pack
{
  int fd; //file descriptor
  struct file *fp; //file pointer
  struct list_elem elem;
};

void syscall_init (void);

struct flist_pack* fd_to_flist_pack(int fd);

#endif /* userprog/syscall.h */

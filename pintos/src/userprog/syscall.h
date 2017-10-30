#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

static void check_addr_safe(const void *vaddr);
void exit(int status);
#endif /* userprog/syscall.h */

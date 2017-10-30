#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
static void syscall_handler (struct intr_frame *);
static void check_addr_safe(void *vaddr);
#endif /* userprog/syscall.h */

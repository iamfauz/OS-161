#ifndef _PROC_SYSCALLS_H_
#define _PROC_SYSCALLS_H_

#include <mips/trapframe.h>

/*---Proccess related syscall prototypes---- */

pid_t sys_fork(struct trapframe *tf, int *retval); /* fork */

pid_t sys_getpid(int *retval); /* getpid */

pid_t sys_waitpid(pid_t pid, userptr_t status, int options, int *retval); /* waitpid */

void sys_exit(int exitcode); /* exit */

int sys_execv(userptr_t progname, userptr_t args); /* execv */

#endif /* _PROC_SYSCALLS_H_ */

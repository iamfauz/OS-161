#ifndef _FILE_SYSCALLS_H_
#define _FILE_SYSCALLS_H_



/*---File related syscall prototypes---- */

int sys_open(userptr_t filename, int flags, int *retval); //open

int sys_read(int fd, userptr_t buf, size_t buflen, int *retval); //read

int sys_write(int fd, userptr_t buf, size_t buflen, int *retval); //write 

int sys_close(int fd); //close

off_t sys_lseek(int fd, off_t pos, int whence, off_t *retval); //lseek 

int sys_dup2(int oldfd, int newfd, int *retval); //dup2

int sys_chdir(userptr_t buf, int *retval); //chdir

int sys___getcwd(userptr_t buf, size_t buflen, int *retval); //_getcwd




#endif /* _FILE_SYSCALLS_H_ */

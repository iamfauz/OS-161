#include <types.h>
#include <file_syscalls.h>
#include <limits.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/syscall.h>
#include <kern/errno.h>
#include <proc.h>
#include <synch.h>
#include <spinlock.h>
#include <uio.h>
#include <copyinout.h>
#include <kern/stat.h>
#include <kern/iovec.h>
#include <kern/fcntl.h>
#include <current.h>
#include <vm.h>
#include <kern/seek.h> 
#include <proc_table.h>
#include <proc_syscalls.h>
#include <addrspace.h>
#include <mips/trapframe.h>
#include <syscall.h>
#include <kern/wait.h>
#include <file_table.h>

//Pointers should be alligned by 4 bytes, this constant is used in execv
#define PTR_ALIGNMENT 4

/*------------Process - related syscalls definitions-------------*/ 

/* fork */
pid_t sys_fork(struct trapframe *tf, int *retval){

 
 int result; // used for error handling 
 
 /* Create new child process */
 struct proc *child_proc;
 child_proc = proc_create_runprogram("[user]");
 
 if(child_proc == NULL) //if NULL, then pid limit has reached
     return EMPROC;
 
 child_proc->parent_pid = curproc->pid; //assiging parent id to the child proc
 
 /* Copying address space using as_copy */
 result = as_copy(proc_getas(), &child_proc->p_addrspace);
 if(result){
   proc_destroy(child_proc);
   return result;
   
 }
 
 /* Closing std files, to avoid wasting memory */
 struct file_obj *f_obj_in = child_proc->file_table[0];
 struct file_obj *f_obj_out = child_proc->file_table[1];
 struct file_obj *f_obj_err = child_proc->file_table[2];
 
 file_obj_close(f_obj_in);
 file_obj_close(f_obj_out);
 file_obj_close(f_obj_err);
 
 /* Copying the filetable */
 int i;
 for( i = 0; i < OPEN_MAX ; i++){
  
  child_proc->file_table[i] = curproc->file_table[i];
  
  struct file_obj *f_obj;
  f_obj = child_proc->file_table[i];
  
  //Increasing refcount if not NULL
  if(f_obj != NULL){
   
   lock_acquire(f_obj->f_lock);
   
   f_obj->refcount = f_obj->refcount + 1;
   
   lock_release(f_obj->f_lock);
   } 
 }
 
 /* Copying trapframe pointer and modifying it appropiately for child process. 
  * Note - modification of trapframe is done in enter_forked_process() in syscall.c in arch/mips/syscall*/
 struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
 memcpy(child_tf, tf, sizeof(struct trapframe));
 
 /* Warp to user mode */
 result = thread_fork(child_proc->p_name, child_proc, &enter_forked_process,  child_tf, (unsigned long)child_proc->p_addrspace); 
 if(result){
 
   kfree(child_tf);
   proc_destroy(child_proc);
   return result;
 }


 *retval = child_proc->pid; //for parent proccess
 return 0;

}


/* getpid */
pid_t sys_getpid(int *retval){

  *retval = curproc->pid;
  
  return 0;
}


/* waitpid */
pid_t sys_waitpid(pid_t pid, userptr_t status, int options, int *retval){
 
 //Checking if options flag is valid
 if(options != 0)
    return EINVAL;
 
 //Checking if pid is valid 
 if(pid < 0 || pid > PID_MAX)
    return ESRCH;
  
 if(proc_table[pid] == NULL)
    return ESRCH;
 
 //Checking is pid is actually the child of the curproc 
 if(proc_table[pid]->parent_pid != curproc->pid)
    return ECHILD;
    
 //Checking if status is valid  
 if(status == (void *) 0x80000000 || status == (void *) 0x40000000 || status == (void *) 0x0000003f )
    return EFAULT; 
      
 struct proc *child_proc = proc_table[pid];
  
 lock_acquire(waitpid_lk);
  
    while(child_proc->exit_flag == false){
    
    cv_wait(exit_cv, waitpid_lk);
   
    }
    
    copyout((void *)&child_proc->exit_code, status, sizeof(int));
    
 lock_release(waitpid_lk);
  
  proc_destroy(child_proc); 
  proc_table[pid] = NULL;

  *retval = pid;
  return 0;

}

/* exit */
void sys_exit(int exitcode){

  
 lock_acquire(waitpid_lk);
    
    //Setting exit flags 
    curproc->exit_flag = true;
    curproc->exit_code = _MKWAIT_EXIT(exitcode);
    
    cv_broadcast(exit_cv, waitpid_lk);
   
 lock_release(waitpid_lk);

   thread_exit();

}

/* execv */
int sys_execv( userptr_t progname, userptr_t args){
  
  /*1. we need to copy args from userspace to the kernel buffer
    Each user arg is terminated by '\0'
    The array of user args is terminated by NULL*/
      
  //checking if the program name is valid
  if(progname == NULL)
   return EFAULT;
  
  //Checking if args is valid 
  if(args == (void *) 0x80000000 || args == (void *) 0x40000000 || args == (void *) 0x0000003f )
    return EFAULT; 
  
  int argc = 0; //used to keep track of the number of args
  char **kernel_args; //this is a kernel buffer that will hold argument strings
  int i;
  int result; //used for error handling
  
  /*Use copyin to copy each pointer starting at args[0]*/
  while(1){
    char *ptr;
    result = copyin( (args + (argc * PTR_ALIGNMENT) ), &ptr, sizeof(ptr));
    if(ptr == NULL || result != 0)
      {
	break;
      }
    argc++;
  }
  
  
  if(argc == 0)
   return EFAULT;
   
  //Checkinging if no of arguments exceed limit
  if(argc > ARG_MAX)
   return E2BIG;
   
   
  char *kprogname; //This will hold the program name copied in
  char *pptr; //A pointer to program name string
  size_t progname_size;


  //Copy in the program name pointer
  result = copyin(progname, &pptr, sizeof(char *));
  if(result)
    return result;
  
  kprogname = kmalloc(__PATH_MAX);
  
  //Copy in the actual program name string
  result = copyinstr((const_userptr_t)progname, kprogname, __PATH_MAX, &progname_size);
  if(result){
   kfree(kprogname);
   return result;
  }
  
  //Checking if progname is not empty 
  if(strlen(kprogname) == 0){
   kfree(kprogname);
   return EINVAL;
  
  }
  
  kernel_args = kmalloc(argc * sizeof(char *));
  //This array will hold the lengths of argument strings
  size_t *arg_sizes = kmalloc((argc) * sizeof(size_t));
  //This is an array of pointers to the arguments
  char **arg_ptrs = kmalloc((argc) * sizeof(char *));
  
  /*Copy in each pointer, get the size of the argument, allocate the correct size,
    copy in the argument string in to the kernel buffer and store the size copied in*/
  for(i = 0; i<argc; i++){
    
    size_t temp_size;

    result = copyin( (args + (i * PTR_ALIGNMENT) ), &arg_ptrs[i], PTR_ALIGNMENT);
    if(result){
    return result;
    }
    
    if(arg_ptrs[i] == (void *) 0x80000000 || arg_ptrs[i] == (void *) 0x40000000 || arg_ptrs[i] == (void *) 0x0000003f )
     return EFAULT;
    
    size_t real_size = strlen(arg_ptrs[i]) + 1;
   
    kernel_args[i] = kmalloc(real_size);
    result = copyinstr((const_userptr_t)arg_ptrs[i], kernel_args[i], real_size, &temp_size);
    if(result){
    return result;
    }
    
    arg_sizes[i] = temp_size; 
  }


  //a vnode for opening the file
  struct vnode *v;
  vaddr_t entrypoint, stackptr;

  //Open the file, giving it the program name and vnode
  result = vfs_open(kprogname, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }

  //get a new address space
  struct addrspace *new_addrspace = NULL;
  struct addrspace *old_addrspace = NULL;
  old_addrspace = curproc->p_addrspace;
  new_addrspace = as_create(); 

 
  //switch to new address space
  proc_setas(new_addrspace);
  as_activate();

  //load a new executable
  result = load_elf(v, &entrypoint);
  if (result) {
    return result;
  }
  
  //close file
  vfs_close(v);
  
  //define a new stack region
  result = as_define_stack(new_addrspace, &stackptr);
  if (result) {
    return result;
  }
  
  /*copy args to new address space
    An array of pointer to args , will be placed below
    args on stack*/
  vaddr_t *uargv;
  //we allocate enough space for an extra pointer that will point to NULL
  uargv = kmalloc((argc + 1) * sizeof(char *)); 
  size_t argn_size;

  for (i = argc - 1 ; i>=0; i--){
    //first we adjust the stackptr to make space for an ALIGNED arg
    argn_size = arg_sizes[i];
    stackptr = stackptr - (argn_size + (PTR_ALIGNMENT - (argn_size % PTR_ALIGNMENT)));
    //copy the arg string to the stack
    copyoutstr(kernel_args[i], (userptr_t)stackptr, argn_size, NULL);
    //store the address of this arg 
    uargv[i] = stackptr;
   }
  
  uargv[argc] = (int)NULL;//the final arg is NULL, therefore we point to NULL
  
  //copy out the pointers to the arguments
  for(i = argc; i>=0; i--){
    stackptr = stackptr - PTR_ALIGNMENT;
    copyout(&uargv[i], (userptr_t)stackptr, sizeof(char *));
  }
 
  //free up space we kmalloced for the kernel buffer, program name.
  for(i = 0; i<argc; i++){
    kfree(kernel_args[i]);
  }
  
  kfree(kernel_args);
  
  //finally it is safe to destroy the old address space
  as_destroy(old_addrspace);
  
  //go to usermode
  enter_new_process(argc, (userptr_t)stackptr, NULL, stackptr, entrypoint);
  return EINVAL;
  
}

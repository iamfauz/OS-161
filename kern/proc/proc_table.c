#include <types.h>
#include <file_syscalls.h>
#include <limits.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
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
#include <addrspace.h>



/* Method that returns the new pid 
 * Returns -1 if no max limit of pids has reached 
 */
pid_t get_pid(){


 int i;
 
 int pid = -1;
 //Get first available slot in the proccess table 
 for(i = PID_MIN ; i <= PID_MAX ; i++){
      
      if(proc_table[i] == NULL){
        
        pid = i;
        break;
      
      }
  }
 
 return pid;

}





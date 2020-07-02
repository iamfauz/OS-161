
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
#include <file_table.h>


/*------------File-related syscalls definitions-------------*/ 

/* open */
int sys_open(userptr_t filename, int flags, int *retval){
      
   //Checking if flags are correct
   int flag_check;
   flag_check = flags & O_ACCMODE;
   
   if( (flag_check != 0) && (flag_check != 1) && (flag_check != 2)){
      
      return EINVAL;
    }
   
   //getting first closed element in the file table and hence the fd
   int fd = -1;
   int i;
   for(i = 3 ; i < OPEN_MAX ; i++){ 
     if(curproc->file_table[i] == NULL){
        
        fd = i;
        break;
        
       }
     }
    
    if(fd == -1){
      
     return EMFILE;
    
    }
   
   /* Getting pathname from userspace */
   char *kfilename = kmalloc(PATH_MAX * sizeof(char));
   size_t *len = kmalloc(sizeof(int));
   
   int result;
   
   result = copyinstr((const_userptr_t)filename, kfilename, PATH_MAX, len); //user buf to kernel buf
   if(result){
   
    kfree(kfilename);
    return result;
   
   }
    
   /* Creating and initializing the file object */
    struct file_obj *f_obj;
    f_obj = kmalloc(sizeof(struct file_obj));
    struct vnode *vn;
    
    result = vfs_open(kfilename, flags, 0664, &vn ); //init vnode
    if(result){
    
    kfree(kfilename);
    return result;
    
    }
    
    file_obj_init(f_obj, kfilename, 0, flags, vn); 
   
    //Adding the file obj to filetable
    curproc->file_table[fd] = f_obj;
   
    kfree(kfilename);
    *retval = fd;
    return 0;
    
}

/* read */
int sys_read(int fd, userptr_t buf, size_t buflen, int *retval){

  if(fd < 0 || fd >= OPEN_MAX){ //Checking if fd is valid
  
     return EBADF;
  }
  
  if(curproc->file_table[fd] == NULL){ //Checking if file is not opened
     
     return EBADF;
  } 
  
  //Getting a reference to the file object from the file table
  struct file_obj *f_obj;
  f_obj = curproc->file_table[fd];
  
  if((f_obj->flags & O_ACCMODE) == O_WRONLY){ //check if file is not opened in WRITE mode
    
    return EBADF;
  }
  
  int result;
  
  lock_acquire(f_obj->f_lock);
  
   struct iovec iov;
   struct uio uio;
 
   uio_uinit(&iov, &uio, buf, buflen, f_obj->offset, UIO_READ);
   
   result = VOP_READ(f_obj->vn, &uio);
   if(result){
   
   lock_release(f_obj->f_lock);
   return result;
   
   }
   
   *retval = uio.uio_offset - f_obj->offset; //amount read
   f_obj->offset = uio.uio_offset; //update file offset
   
  lock_release(f_obj->f_lock);
  
  return 0;


}

/* write */
int sys_write(int fd, userptr_t buf, size_t buflen, int *retval){
  
  if(fd < 0 || fd >= OPEN_MAX){ //Checking if fd is valid
  
     return EBADF;
  }
  
  if(curproc->file_table[fd] == NULL){ //Checking if file is not opened
     
     return EBADF;
  } 
  
  //Getting a reference to the file object from the file table
  struct file_obj *f_obj;
  f_obj = curproc->file_table[fd];
 
  if((f_obj->flags & O_ACCMODE) == O_RDONLY){ //check if file is not opened in READ mode
    
    return EBADF;
  }
  
  int result;
  
  lock_acquire(f_obj->f_lock);
  
   struct iovec iov;
   struct uio uio;
   
   uio_uinit(&iov, &uio, buf, buflen, f_obj->offset, UIO_WRITE);
   
   
   result = VOP_WRITE(f_obj->vn, &uio);
   
   if(result){
   
   
   lock_release(f_obj->f_lock);
   return result;
   
   }
   
   *retval = uio.uio_offset - f_obj->offset; //amount written
   f_obj->offset = uio.uio_offset; //update file offset
  
  lock_release(f_obj->f_lock);
  
  
  return 0;

}

/* close */
int sys_close(int fd){


  if(fd < 0 || fd >= OPEN_MAX){ //Checking if fd is valid
  
     return EBADF;
  }
  
  if(curproc->file_table[fd] == NULL){ //Checking if file is not opened
     
     return EBADF;
  } 

  //Getting a reference to the file object from the file table
  struct file_obj *f_obj;
  f_obj = curproc->file_table[fd];
  
  //Closing file
  file_obj_close(f_obj);
  
  curproc->file_table[fd] = NULL;
  
  return 0;

}

/* lseek */
off_t sys_lseek(int fd, off_t pos, int whence, off_t *retval){

  
  if(fd < 0 || fd >= OPEN_MAX){ //Checking if fd is valid
  
     return EBADF;
  }
  
  if(curproc->file_table[fd] == NULL){ //Checking if file is not opened
     
     return EBADF;
  } 
  
  if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END){ //Checking if whence is valid or not
  
     return EINVAL;
  
  }
   
  //Getting a reference to the file object from the file table
  struct file_obj *f_obj;
  f_obj = curproc->file_table[fd];
  
  
  if(!VOP_ISSEEKABLE(f_obj->vn)){ //checking if ffile is seekable
     
     return ESPIPE;
  }
   
  int result;
  
  lock_acquire(f_obj->f_lock);
  
  off_t seek; //new position to seek
  struct stat *f_info = kmalloc(sizeof(struct stat));
  
    switch(whence){
    
    case SEEK_SET:
    
           seek = pos;
           
           break;
           
    case SEEK_CUR:
           
           seek = f_obj->offset + pos;
    
          break;
          
    case SEEK_END:
           
           result = VOP_STAT(f_obj->vn, f_info);
           
           if(result){
           
           kfree(f_info);
           lock_release(f_obj->f_lock);
           return result;
           
           }
           
           seek = f_info->st_size + pos;
           
           break;
          
     default: //never reached
     
          seek = -1;
          break;
                              
   }
  
    kfree(f_info);
    
   if(seek < 0){ //Checking if seek pos is negative
     
     lock_release(f_obj->f_lock);
     return EINVAL;
     
   }  

   f_obj->offset = seek; //updating offset to new position
   *retval = seek;
  
   lock_release(f_obj->f_lock);
   
   return 0;
}


/* dup2 */
int sys_dup2(int oldfd, int newfd, int *retval){


 if(oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX){ //Checking if oldfd and newfd is valid
  
     return EBADF;
  }
  
  if(curproc->file_table[oldfd] == NULL){ //Checking if file is not opened
     
     return EBADF;
  } 

  if(oldfd == newfd){ //do nothing if equal
  
    *retval = newfd;
    return 0;
  
  }
  
  spinlock_acquire(&curproc->p_lock); //Aquiring process lock
     
     int result;
     
     /*if file at newfd is not closed, close it */
     if(curproc->file_table[newfd] != NULL){
        
        result = sys_close(newfd);
        if(result){ //if close fails
           spinlock_release(&curproc->p_lock);
           return result;
        
       } 
     }
     
     //Getting a reference to the old file object from the file table
     struct file_obj *f_obj_old;
     f_obj_old = curproc->file_table[oldfd];
     
     lock_acquire(f_obj_old->f_lock);
     f_obj_old->refcount = f_obj_old->refcount + 1; //increasing ref count
     lock_release(f_obj_old->f_lock);
  
     curproc->file_table[newfd] = f_obj_old; //dup
    
   spinlock_release(&curproc->p_lock); //Releasing process lock
  
  *retval = newfd;
  return 0;
  
}


/* chdir */
int sys_chdir(userptr_t buf, int *retval){

     /* Getting pathname from userspace */
     char *kbuf = kmalloc(PATH_MAX * sizeof(char));
     size_t *len = kmalloc(sizeof(int));
     
     int result; 
     result = copyinstr((const_userptr_t)buf, kbuf, PATH_MAX, len); //user buf to kernel buf //copy user buf to kernel buf
     if(result){
   
     kfree(kbuf);
     return result;
  
     }
     
     result = vfs_chdir((char *) kbuf);
     if(result){
   
     kfree(kbuf);
     return result;
  
     }
     
     kfree(kbuf);
     *retval = 0;
     
     return 0;
     
}

/* _getcwd */
int sys___getcwd(userptr_t buf, size_t buflen, int *retval){

   void *kbuf = kmalloc(buflen);
   int result;
  
   struct iovec iov;
   struct uio uio;
 
   uio_kinit(&iov, &uio, kbuf, buflen, 0, UIO_READ);
   
   result = vfs_getcwd(&uio);
   
   if(result){
   
   kfree(kbuf);
   return result;
   
   }
   
   *retval = buflen - uio.uio_resid; //amount read
  
   result = copyout(kbuf, buf, buflen); //copy kernel buff to user buf
   if(result){
   
    kfree(kbuf);
    return result;
  
   }
  
   kfree(kbuf);
   return 0;
 
}















#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file_table.h>

/*
 * Method to initialize file object
 */
int file_obj_init(struct file_obj *f_obj, char* filename, int offset, int flags, struct vnode *vn)
{
   f_obj->vn = vn;
   f_obj->offset = offset;
   f_obj->mode = 0664; //not required
   f_obj->refcount = 1;
   f_obj->f_lock = lock_create(filename);
   f_obj->flags = flags;
   
   return 0;
}

/*
 * Method that closes file object i.e destroys file object if reference count is 0
 */
int file_obj_close(struct file_obj *f_obj)
{
  
  if(f_obj == NULL)
   return -1;
   
  lock_acquire(f_obj->f_lock);
  
  f_obj->refcount = f_obj->refcount - 1; //Reducing refcount
  
  //if no references left, close file and cleanup memory
  if(f_obj->refcount == 0){ 
    
     vfs_close(f_obj->vn); //close file and destroy vn
     lock_release(f_obj->f_lock);
     lock_destroy(f_obj->f_lock); //destroy lock
     kfree(f_obj); //free file object
  }
  else{
  
     lock_release(f_obj->f_lock);
  
  }
  
  f_obj = NULL;
   
   
  return 0;
}

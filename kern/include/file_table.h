#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

#include <types.h>
#include <spinlock.h>
#include <limits.h>
#include <synch.h>
#include <vnode.h>

/*
 * A struct to represent a file object
 *
 */
 struct file_obj {
 
  struct vnode *vn; 
  off_t offset;
  mode_t mode;
  int flags;
  int refcount; /* no of references */
  struct lock *f_lock;
 
};
 
/*
 * Method that initializes the file_object
 */ 
 int file_obj_init(struct file_obj *f_obj, char* filename, int offset, int flags, struct vnode *vn);


/*
 * Method that closes file object i.e destroys file object if reference count is 0
 */
int file_obj_close(struct file_obj *f_obj);





#endif /* _FILE_TABLE_H_ */

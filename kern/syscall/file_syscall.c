#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <kern/unistd.h>
#include <lib.h>
#include <vnode.h>
#include <copyinout.h>
#include <vfs.h>
#include <limits.h>
#include <uio.h>
#include <proc.h>
#include <kern/errno.h>
#include <current.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <kern/stat.h>

#define SYSTEM_OPEN_MAX (10*OPEN_MAX)

struct fdesc system_fdtable[SYSTEM_OPEN_MAX];

/*
 * syscall for open a file, on success returns the file descriptor
 */
int
sys_open(userptr_t fpath, int openflags, mode_t mode, int* errp)
{
  struct vnode* v;
  struct fdesc* fopen = NULL;
  char * path = (char*) fpath; 
  static int result, fd;
  int i = 0;

  if ( path == NULL ) {
    *errp = EFAULT;
    return -1;
  } 
  result = vfs_open(path, openflags, mode, &v);
  if (result){
    *errp = ENOENT;
    return -1;
  }
  // check if there's a free entry in system_fdtable 
  for(i=STDERR_FILENO+1; i<SYSTEM_OPEN_MAX; i++){
    if ( system_fdtable[i].file_v == NULL){
      fopen = &system_fdtable[i];
      fopen->file_lock = lock_create("f_lock");
      if (fopen->file_lock == NULL){
	*errp = ENOMEM;
	return -1;
      } else {
	fopen->file_name = path;
	fopen->file_offset = 0; // HOW TO MANAGE APPEND ?
	fopen->file_refcount = 1;
	fopen->file_v = v; 
	fopen->file_mode = mode;
	break;
      }
    }
  }
  if (fopen ==  NULL){
    *errp = ENFILE;
    return -1;
  }
  //fd = proc_file_alloc(fopen); // returns fdtable index
  for(fd=STDERR_FILENO+1; fd<OPEN_MAX; fd++){
    if(curproc->p_fdtable[fd] == NULL){
      curproc->p_fdtable[fd] = fopen;
      return fd;
    }
  }
  //if ( fd == -1){
    *errp = EMFILE;
    // to free its entry in system_fdtable 
    vfs_close(v); // will set system_fdtable[i].file_v to NULL 
    return -1;
    //} else {
    // return fd;
    //}
}


/*
 * syscall to close a file
 */
int
sys_close(int fd){
  struct fdesc* f_toclose;
  struct vnode* v;
  if ( fd<0 || fd>OPEN_MAX ) return -1;
  f_toclose = curproc->p_fdtable[fd];
  if ( f_toclose  == NULL ) {
    return -1;
  }
  lock_acquire(f_toclose->file_lock);
  v = f_toclose->file_v;
  curproc->p_fdtable[fd] = NULL;
  f_toclose->file_refcount--;
  if ( f_toclose->file_refcount == 0 )
    {
      lock_release(f_toclose->file_lock);
      lock_destroy(f_toclose->file_lock);
      f_toclose->file_v = NULL;
      if (v==NULL) return -1;
      vfs_close(v);
    }
  lock_release(f_toclose->file_lock);
  return 0;
}

/*
 * Function that writes file indexed by file descriptor fd
 */
static int
file_write(int fd, userptr_t buf_ptr, size_t buf_length)
{
  struct iovec iov;
  struct uio u;
  struct fdesc* of;
  int result, nwrite;
  struct vnode* vn;

  if ( fd<0 || fd > OPEN_MAX) return -1;
  of = curproc->p_fdtable[fd];
  if ( of == NULL ) return -1;
  vn = of->file_v;
  if ( vn == NULL ) return -1;

  iov.iov_ubase = buf_ptr;
  iov.iov_len = buf_length;

  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_resid = buf_length;
  u.uio_segflg = UIO_USERISPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  lock_acquire(of->file_lock);
  u.uio_offset = of->file_offset;
  
  result = VOP_WRITE(vn, &u);
  if (result) {
    lock_release(of->file_lock);
    return result;
  } 
  of->file_offset = u.uio_offset;
  lock_release(of->file_lock);
  nwrite = buf_length - u.uio_resid;
  return(nwrite);
}


/*
 * Syscall to write to a file or to console
 */
int
sys_write(int fd, userptr_t buf_ptr, size_t buf_length)
{
  int i = 0;
  char *to_print = (char*) buf_ptr;
  
  if (fd != STDOUT_FILENO && fd != STDERR_FILENO){
    return file_write(fd, buf_ptr, buf_length);
  }
  for (i=0; i<(int)buf_length; i++){
    putch(to_print[i]);
  }
  return (int)buf_length;
}

/*
 * Function to read from file 
 */
static int file_read(int fd, userptr_t buf_ptr, size_t buf_length)
{
  struct iovec iov;
  struct uio u;
  struct fdesc* of;
  int result, nread;
  struct vnode* vn;

  if ( fd<0 || fd > OPEN_MAX) return -1;
  of = curproc->p_fdtable[fd];
  if ( of == NULL ) return -1;
  vn = of->file_v;
  if ( vn == NULL ) return -1;

  iov.iov_ubase = buf_ptr;
  iov.iov_len = buf_length;
  
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_resid = buf_length;
  u.uio_segflg = UIO_USERISPACE;
  u.uio_rw = UIO_READ;
  u.uio_space = curproc->p_addrspace;

  lock_acquire(of->file_lock);
  u.uio_offset = of->file_offset;
  result = VOP_READ(vn, &u);
  if (result)  {
    lock_release(of->file_lock);
    return result;
  }
  of->file_offset = u.uio_offset;
  nread = buf_length - u.uio_resid;
  return(nread);
}

/*
 * Syscall to read from file or from console
 */
int
sys_read(int fd, userptr_t buf_ptr, size_t buf_length)
{
  
  int i = 0;
  char *to_read = (char*) buf_ptr;
  // Supported only write to stdout and stderr -> fd constants in unistd.h
  if (fd != STDIN_FILENO){
    return file_read(fd, buf_ptr, buf_length);
  }
  for (i=0; i<(int)buf_length; i++){
  to_read[i]=getch();
  // if read eof that is -1 then stop 
    if(to_read[i]<0){ 
      return i;
    }
  }
  return (int)buf_length;
}


/*
 * Syscall to clone file handle of oldfd onto file handle of newfd
 */
int
sys_dup2( int oldfd, int newfd, int* errp)
{
  struct fdesc* f_old;
  int returned = 0;
  if (oldfd < 0 || oldfd > OPEN_MAX || newfd < 0 || newfd > OPEN_MAX) {
    *errp = EBADF;
    return -1;
  }
  f_old = curproc->p_fdtable[oldfd];
  if ( f_old == NULL ) {
    *errp = EBADF;
    return -1;
  }
  if ( oldfd == newfd ) return oldfd;
  lock_acquire(f_old->file_lock);
  if ( curproc->p_fdtable[newfd] != NULL) returned = sys_close(newfd);
  if (returned<0){
    lock_release(f_old->file_lock);
    *errp = returned;
    return -1;
  }
  curproc->p_fdtable[newfd] = f_old;
  f_old->file_refcount++;
  lock_release(f_old->file_lock);
  return newfd;
}

/*
 * Syscall to change the file seek position 
 */
off_t
sys_lseek(int fd, off_t pos, int whence, int *errp)
{
  struct fdesc* f_desc;
  if (fd < 0 || fd >= OPEN_MAX) {
    *errp = EBADF;
    return -1;
  }

  if (curproc->p_fdtable[fd] == NULL) {
    *errp = EBADF;
    return -1;
  }
  
  f_desc = curproc->p_fdtable[fd];
  int new_position = -1;
 
  switch(whence) {

    /* new seek position set to pos */
  case SEEK_SET:

    if (pos < 0) {
      *errp = EINVAL;
      return -1;
    }

    lock_acquire(f_desc->file_lock);
    f_desc->file_offset = pos;
    new_position = f_desc->file_offset;

    if (!VOP_ISSEEKABLE(f_desc->file_v)) {
      lock_release(f_desc->file_lock);
      *errp = ESPIPE;
      return -1;
    }

    lock_release(f_desc->file_lock);
    break;

    /* new file seek position is current position + pos */
  case SEEK_CUR:
    lock_acquire(f_desc->file_lock);

    f_desc->file_offset += pos;
    new_position = f_desc->file_offset;

    if (!VOP_ISSEEKABLE(f_desc->file_v)) {
      lock_release(f_desc->file_lock);
      *errp = ESPIPE;
      return -1;
    }

    lock_release(f_desc->file_lock);
    break;

    /* new file seek position is EOF + pos */
  case SEEK_END:
    lock_acquire(f_desc->file_lock);

    struct stat statbuf;
    int result = VOP_STAT(f_desc->file_v, &statbuf);
    if (result) {
      *errp = result;
      lock_release(f_desc->file_lock);
      return -1;
    }

    f_desc->file_offset = pos + statbuf.st_size;
    if (!VOP_ISSEEKABLE(f_desc->file_v)) {
      lock_release(f_desc->file_lock);
      *errp = ESPIPE;
      return -1;
    }

    new_position = f_desc->file_offset;

    lock_release(f_desc->file_lock);
    break;

  default:
    *errp = EINVAL;
    return -1;
  }

  return new_position;
}



/*
 * Function that initializes console file descriptors
 */
int fd_console_init(void)
{
  struct vnode* v = NULL;
  struct fdesc* f_console = NULL;
  int fd = 0;
  int result; 
  char io[] = "con:";

  // Allocate the first 3 entries of system fd table to console 
  for (; fd<3; fd++){
    if ( system_fdtable[fd].file_v != NULL ) return -1;
    f_console = &system_fdtable[fd];
    f_console->file_lock = lock_create("console_lock");
    if (f_console->file_lock == NULL) return ENOMEM;
    f_console->file_name = io;
    f_console->file_offset = 0;
    curproc->p_fdtable[fd] = &f_console[fd];
    switch(fd) {
      case 0:
	f_console[fd].file_mode = O_RDONLY;
	result = vfs_open(io, O_RDONLY, 0664, &v);
	f_console->file_v = v;
	if (result) return ENODEV;
	f_console->file_refcount = 1;						
	break;
      case 1:
	f_console[fd].file_mode = O_WRONLY;
	result = vfs_open(io, O_WRONLY, 0664, &v);
	f_console->file_v = v;
	if (result) return ENODEV;
	f_console->file_refcount = 1;						
	break;
      case 2:
	f_console[fd].file_mode = O_WRONLY;
	result = vfs_open(io, O_WRONLY, 0664, &v);
	f_console->file_v = v;
	if (result) return ENODEV;
	f_console->file_refcount = 1;						
	break;
     default:
       kprintf("Error while initializing console\n");
       return -1;
    }
    v = NULL;
  }
  return 0;
}

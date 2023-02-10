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

#define SYSTEM_OPEN_MAX (10*OPEN_MAX)

struct fdesc system_fdtable[SYSTEM_OPEN_MAX];

/*
 * syscall for open a file, on success returns the file descriptor
 */
int sys_open(userptr_t fpath, int openflags, mode_t mode, int* errp)
{
  struct vnode* v;
  struct fdesc* fopen = NULL;
  char * path = (char*) fpath; 
  static int result, fd;
  int i = 0;

  if ( path==NULL )  return -1;
  result = vfs_open(path, openflags, mode, &v);
  if (result){
    *errp = ENOENT;
    return -1;
  }
  for(i=0; i<SYSTEM_OPEN_MAX; i++){
    if ( system_fdtable[i].file_v == NULL){
      fopen = &system_fdtable[i];
      fopen->file_name = path;
      fopen->file_offset = 0; // HOW TO MANAGE APPEND ?
      fopen->file_refcount = 1;
      fopen->file_v = v;
      break;
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
    vfs_close(v);
    return -1;
    //} else {
    // return fd;
    //}
}


/*
 * syscall to close a file
 */
int sys_close(int fd){
  struct fdesc* f_toclose;
  struct vnode* v;
  if ( fd<0 || fd>OPEN_MAX ) return -1;
  f_toclose = curproc->p_fdtable[fd];
  v = f_toclose->file_v;
  if ( f_toclose  == NULL ) {
    return -1;
  } else {
    curproc->p_fdtable[fd] = NULL;
    f_toclose->file_refcount--;
    if ( f_toclose->file_refcount == 0 ){
      f_toclose->file_v = NULL;
      if (v==NULL) return -1;
      vfs_close(v);
    }
  }
  return 0;
}

/*
 * Function that writes file indixed by file descriptor fd
 */
static int file_write(int fd, userptr_t buf_ptr, size_t buf_length)
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
  u.uio_offset = of->file_offset;
  u.uio_segflg = UIO_USERISPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  result = VOP_WRITE(vn, &u);
  if (result)  return result;
  of->file_offset = u.uio_offset;
  nwrite = buf_length - u.uio_resid;
  return(nwrite);
}


/*
 * Syscall to write to a file or to console
 */
int sys_write(int fd, userptr_t buf_ptr, size_t buf_length)
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
  u.uio_offset = of->file_offset;
  u.uio_segflg = UIO_USERISPACE;
  u.uio_rw = UIO_READ;
  u.uio_space = curproc->p_addrspace;

  result = VOP_READ(vn, &u);
  if (result)  return result;
  
  of->file_offset = u.uio_offset;
  nread = buf_length - u.uio_resid;
  return(nread);
}

/*
 * Syscall to read from file or from console
 */
int sys_read(int fd, userptr_t buf_ptr, size_t buf_length)
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

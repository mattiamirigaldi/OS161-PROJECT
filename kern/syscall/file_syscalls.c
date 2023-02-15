//implementazione sys_write() and sys_read() come prototipi read and write
//usa putch(), getch() per stdin/out/err
//putch() show char on stdout
//getch() acquire char on stdin
#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <clock.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <proc.h>

#include <copyinout.h>
#include <current.h>
#include <syscall.h>
#include <limits.h>
#include <lib.h> //mai mettere lib prima di types, sennò non compila :)




#if OPT_FILE

#define SYS_MAX (10*OPEN_MAX)
#define use_kernel 0
/* max num of system wide open files */
struct openfile{
	struct vnode *vn;
	off_t offset;
	unsigned int count;
  struct lock *f_lock;
};
struct openfile SYSfileTable[SYS_MAX];

struct fileTable{
   struct openfile *ft_openfiles[OPEN_MAX];
};


//use uio_kinit +VOP read/write+uiomove (to transfer mem btw user&kernel)
#if use_kernel
static int
file_read(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio kernelu;
  int result, nread;
  struct vnode *vn;
  struct openfile *of;
  void *kernelbuf;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  kbuf = kmalloc(size);
  uio_kinit(&iov, &kernelu, kernelbuf, size, of->offset, UIO_READ);
  result = VOP_READ(vn, &kernelu);
  if (result) {
    return result;
  }
  of->offset = kernelu.uio_offset;
  nread = size - kernelu.uio_resid;
  copyout(kernelbuf,buf_ptr,nread); //only kernel
  kfree(kernelbuf);
  return (nread);
}

static int
file_write(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio kernelu;
  int result, nwrite; //change from write
  struct vnode *vn;
  struct openfile *of;
  void *kernelbuf;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  kbuf = kmalloc(size);
  copyin(buf_ptr, kbuf, size);//change from write
  uio_kinit(&iov, &kernelu, kernelbuf, size, of->offset, UIO_WRITE);//change from write UIO
  result = VOP_WRITE(vn, &kernelu);//change from write
  if (result) {
    return result;
  }
  of->offset = kernelu.uio_offset;
  nwrite = size - kernelu.uio_resid;
  //copyout(kernelbuf,buf_ptr,nread); //only kernel
  //kfree(kernelbuf);
  return (nwrite);
}

//use manual version of uio_kinit (load_segment) +VOP read/write
#else
static int
file_read(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio u;
  int result, nread;
  struct vnode *vn;
  struct openfile *of;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  iov.iov_ubase = buf_ptr;
  iov.iov_len = size;

  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_resid = size;          
  u.uio_offset = of->offset;
  u.uio_segflg =UIO_USERISPACE; //only 
  u.uio_rw = UIO_READ;
  u.uio_space = curproc->p_addrspace; //only 
  result = VOP_READ(vn, &u);
  if (result) {
    return result;
  }

  of->offset = u.uio_offset;
  nread = size - u.uio_resid;
  return (nread);
}

static int
file_write(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio u;
  int result, nwrite;
  struct vnode *vn;
  struct openfile *of;

  if (fd<0||fd>OPEN_MAX) return -1;
  of = curproc->fileTable[fd];
  if (of==NULL) return -1;
  vn = of->vn;
  if (vn==NULL) return -1;

  iov.iov_ubase = buf_ptr;
  iov.iov_len = size;

  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_resid = size;          
  u.uio_offset = of->offset;
  u.uio_segflg =UIO_USERISPACE; //only 
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace; //only 
  result = VOP_WRITE(vn, &u);
  if (result) {
    return result;
  }

  of->offset = u.uio_offset;
  nwrite = size - u.uio_resid;
  return (nwrite);
}
#endif

//per processo user: table of pointers to vnode, save pointer a vnode per ogni file create
//no double table to share data btw user & kernel
int sys_open(int openflags, userptr_t path, mode_t mode, int *err){
  struct vnode *vn;
  struct openfile *of=NULL;
  int result;
  int i, fd;

  result = vfs_open((char *)path, openflags, mode, &vn);
  if (result) {
    *err= ENOENT;
    return -1;
  }
  //initialize
  of->f_lock=lock_create("lock file");
  if (of->f_lock==NULL) { // no free slot in system open file table
    vfs_close(vn);
    kfree(of);
    return ENOMEM;
  }

  for (i=0; i<SYS_MAX; i++) {
    if (SYSfileTable[i].vn==NULL) {
      of = &SYSfileTable[i];
      of->vn = vn;
      of->offset = 0; // handle offset with append
      of->count = 1;
      break;
    }
  }
  if (of==NULL) { // no free slot in system open file table
    vfs_close(vn);
    lock_destroy(of->f_lock);
    *err= ENOMEM;
  }
  //handle modes...........
  else {
    for (fd=STDERR_FILENO+1; fd<OPEN_MAX; fd++) {
      if (curproc->fileTable[fd] == NULL) {
	        curproc->fileTable[fd] = of;
	        result=fd;
      }
    }
    // no free slot in process open file table
    *err= ENOMEM;
    result=ENOMEM;
  }
  if (result) {
    lock_destroy(of->f_lock);
    kfree(of);
    vfs_close(vn);
    return result;
  }
  return 0;

}
int sys_close(int filehandle){
//vfs_close in runprogram
  struct vnode *vn;
  struct openfile *of;
  

  if (filehandle<0||filehandle>OPEN_MAX) return -1; //n° max of open files per proc, file descriptor always>0
  of = curproc->fileTable[filehandle];
  if (of==NULL) return -1;
  

  lock_acquire(of->f_lock);
  vn = of->vn;
  //of->vn=NULL;
  //if (vn==NULL) return -1;
  //LAST CLOSE, FREE EVERYTHING
  if ((of->count)==1) {
    lock_release(of->f_lock);
    lock_destroy(of->f_lock);
    if (vn==NULL) return -1;
    vfs_close (vn); //close file 
    kfree(of);

  }
  else {
    KASSERT((of->count)>1);
    of->count=(of->count)-1;
    lock_release(of->f_lock);

  }
  curproc->fileTable[filehandle]=NULL;
  return 0;

}

//LOOK AT LOADELF.C
//if no kernel space: define in uio userspace and segflag
//struct uio;    /* kernel or userspace I/O buffer (uio.h) */

#endif

//size_t is the  size of buf
//buf is a const * 
int sys_read (int filehandle, userptr_t buf, size_t size){
	int num;
	char *stampato = (char *)buf;
	//to handle read you have have STDIN file, otherwise error
	if (filehandle!=STDIN_FILENO){
    #if OPT_FILE
      return file_read(filehandle, buf, size);
    #else
		kprintf("stdin support, no altri\n");
		return -1;
    #endif
	}
	for(num=0; num<(int)size; num++){
		stampato[num]= getch();
		if (stampato[num]<0) return num;
	}
	return (int)size;
}


int sys_write (int filehandle,userptr_t	buf, size_t size){
        int num;
        char *stampato = (char *)buf;
        //to handle write you have have STDOUT file, otherwise error
        

        if (filehandle!=STDOUT_FILENO && filehandle!=STDERR_FILENO){
          #if OPT_FILE
           return file_write(filehandle, buf, size);
          #else
                kprintf("stdout support, no altri\n");
                return -1;
          #endif
        }
        for(num=0; num<(int)size; num++){
                putch(stampato[num]);
        }
        return (int)size;
}


/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys__exit.
 * It just avoids crash/panic. Full process exit still TODO
 * Address space is released
 */

#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <synch.h>
#include <current.h> //curthread, curproc


/*
 * simple proc management system calls
 */
void
sys__exit(int status)
{
   //int waitpid (pid_t pid, userptr_t returncode, int flags)
  
  
  //AVOID RACE CONDITION WITH REMTHREAD
  /*if thread exit not start and proc destroy needs no thread: race 
  kill process by yourself by remthread calling*/
  
  //signal(..) sem or cv TO PROC_WAIT
  #if opt_waitpid
  //include current
  struct thread *cur=curthread;
  struct proc *p =curproc;

	p->p_status =status &0xFF; //8LSB status
  proc_remthread(cur); //remove thread --> 
  //-->on thread exit use this case ad detached thread, not always

  V(p->p_semaphore);
  
  //return status_wait when signal arrives
  //DON'T NEED TO DESTROY PROC HERE BECAUSE IT IS DONE BY
  //PASSING TO PROC WAIT (it has inside proc destroy)
  #else
  /* get address space of current process and destroy */
  struct addrspace *as = proc_getas();
  as_destroy(as);
  #endif
  /* thread exits. proc data structure will be lost */
  thread_exit();

  panic("thread_exit returned (should not happen)\n");
  (void) status; // TODO: status handling
}

int
sys_waitpid(pid_t pid, userptr_t returncode, int flags)
{
  #if opt_waitpid
  #else
  (void)flags;
  (void)pid;
  (void)returncode;
  return -1;
#endif
}
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
#include <limits.h>


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
/*
kern/include/kern/limits.h
__PID_MIN       2
__PID_MAX       32767
kern/include/limits.h : 
PID_MIN, PID_MAX 
make a table: IN PROC.C (pid, *proc). new proc new line, removed proc removed line
*/
  struct proc *pr;
  int status_pr;
/*DA METTERE IN PROC.C PERCHE TABLE DEFINITA LI, STATIC
  KASSERT(pid>=0 && pid< MAX_PROC+1);
  pr=processTable.proc[pid];
  KASSERT(pr->p_pid==pid);
*/
  pr=from_pid_to_proc(pid);
  if (pr==NULL) return -1;

  (void)flags; // TO HANDLE
  status_pr=proc_wait(pr);
  if (returncode!=NULL) *(int*)returncode= status_pr;
  
  return (int)pid;
  #else
  (void)flags;
  (void)pid;
  (void)returncode;
  return -1;
#endif
}

//TEST WAITPID WITH testbin/forktest, BUT GETPID NEEDED FOR TEST
pid_t 
sys_getpid(void){
  #if opt_waitpid
    KASSERT(curproc!=NULL);
    struct proc *pro= curproc;
    pid_t pid=pro->p_pid;
  return pid;
  #else
  return -1;
#endif
}

/*FORK: clonare intero addr space of parent process to child, and start it*/
//int sys_fork(void){};
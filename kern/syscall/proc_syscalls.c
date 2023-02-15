/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys__exit.
 * It just avoids crash/panic. Full process exit still TODO
 * Address space is released
 */

#include <types.h>
#include <kern/unistd.h>

#include <kern/errno.h> //fork errors
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <synch.h>
#include <mips/trapframe.h>
#include <current.h> //curthread, curproc
#include <limits.h>



//if static voi no cite on file.h
#if OPT_FORK
void 
enter_forked_process_syscall(void *tf_pass,unsigned long data2){
	//trapframe saving in stack
	struct trapframe *c_tf = (struct trapframe*) tf_pass; //kernel stack trapframe copy
	struct trapframe store_tf;
  struct addrspace *c_addr= (struct addrspace*) data2;
	
	c_tf->tf_v0=0; //0 return
	c_tf->tf_a3=0; //success
	c_tf->tf_epc += 4;

	memcpy(&store_tf, c_tf, sizeof(struct trapframe));
	kfree(c_tf);
	c_tf=NULL;

	curproc->p_addrspace=c_addr;
	as_activate();
	mips_usermode(&store_tf);

  panic("no return should be considered after entering fork\n");
}
#endif


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
  #if OPT_WAITPID
  //include current
  
  struct proc *p =curproc;

	p->p_status =status &0xFF; //8LSB status
  proc_remthread(curthread); //remove thread --> 
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
  #if OPT_WAITPID
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
  #if OPT_WAITPID
    KASSERT(curproc!=NULL);
    struct proc *pro= curproc;
    pid_t pid=pro->p_pid;
  return pid;
  #else
  return -1;
#endif
}

/*FORK: clonare intero addr space of parent process to child, and start it*/
//int sys_fork(trapframe, retval){};
#if OPT_FORK

int 
sys_fork(struct trapframe *ctf, pid_t *retval){
  struct thread *cur=curthread;
  struct trapframe *c_tf=NULL;
  struct proc *c_proc=NULL;
  struct addrspace* c_addr=NULL;
  int res_fork;

  //KASSERT(curproc!=NULL);

  //parent trapframe copy (child trapframe creation)
  c_tf=kmalloc(sizeof(struct trapframe)); //allocate space
  if (c_tf==NULL){
    //kfree(c_tf);
    //proc_destroy(c_proc);
    //kprintf("\nvivo\n");
    *retval=ENOMEM;
    return -1;
  }
  memcpy(c_tf, ctf, sizeof(struct trapframe));//copy


  //duplicate addr space (cp cur addr space to new addr space)
  as_copy(curproc->p_addrspace,&c_addr);
  if(c_addr==NULL) {
    kfree(c_tf);
    //proc_destroy(c_proc);
    *retval=ENOMEM;
    return -1;
  
  }

  //new process creation
  c_proc=proc_create_runprogram(curproc->p_name);
  if(c_proc==NULL) {
    kfree(c_tf);
    //proc_destroy(c_proc);
    *retval=ENOMEM;
    return -1;
  }

  /*link to child:
 * Create a new thread based on an existing one.
 * The new thread has name NAME, and starts executing in function
 * ENTRYPOINT. DATA1 and DATA2 are passed to ENTRYPOINT.
 *
 * The new thread is created in the process P. If P is null, the
 * process is inherited from the caller. It will start on the same CPU
 * as the caller, unless the scheduler intervenes first.

  //int thread_fork(const char *name, struct proc *proc, void (*func)(void *, unsigned long),void *data1, unsigned long data2);
  */

  res_fork= thread_fork(cur->t_name, c_proc,enter_forked_process_syscall,c_tf, (unsigned long)c_addr);
  //if PARENT return destroy process and free trapframe OF CHILD
  if (res_fork){
    proc_destroy(c_proc);
    kfree(c_tf);
    return ENOMEM;

  }
  //ritorna il child pid
  *retval =c_proc->p_pid;
  return res_fork;
}
#endif
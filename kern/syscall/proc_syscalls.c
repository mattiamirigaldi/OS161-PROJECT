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
#include <kern/wait.h> //waitpid options ERRORS
#include <kern/fcntl.h> //file constant modes rwoc
#include <vnode.h>
#include <vfs.h>


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

//INITIAL CHECKS FOR WAITPID BADCALLS
int *ret_c = (int *)returncode;
    //invalid pointers:
    if (returncode==NULL) return -1;

    if(ret_c == (int*) 0x40000000 || ret_c == (int*) 0x80000000 || ((int)ret_c & 3) != 0) {
      return -1;
    }
    //invalid options:
    if(flags != 0 &&  flags != WNOHANG && flags != WUNTRACED){
      return -1;
    }
    //pid number invalid: we can choose also PID>0&&PID<=MAX_PID
    //defined in function "from pid to proc"
    if(pid < PID_MIN || pid > PID_MAX) {
      return -1;
    }
   
  pr=from_pid_to_proc(pid);

  //null process pointed by pid
  if (pr==NULL) return -1;
    //NOT HANDLED: ONLY PARENTS CAN CHECK FOR THEIR CHILD PID
   //if(curproc->pid != parent_pid del pid  ){return -1; }

  
  //if (returncode==INVAL_PTR) return -1;
  //kprintf("porcatroia\n");
  (void)flags; // TO HANDLE
  status_pr=proc_wait(pr);
  if (returncode!=NULL) {
    *(int*)returncode= status_pr;}
  else return -1;
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

int 
sys_execv (char* progr, char **args){
  struct addrspace* new_as=NULL;
  struct addrspace* old_as;
  struct vnode *v_exe;
  int res_executable, res_k2u;
  vaddr_t entrypoint, stackptr;
  int args_counter=0;
  int args_counter_copy = 0;
  int asize = 0;
  int padding = 0;

  //CHECK PASSED PARAMETERS
    //invalid arguments
   if(args==NULL || (int*)args== (int*) 0x40000000 || (int*)args== (int*) 0x80000000 ) {
      return -1;
    }
  if (progr==NULL) return -1;

  KASSERT(proc_getas() != NULL);

//copy args from usr space to kernel buf:
/////////////////////////////////////////////
//char **args: pointers to usr space string--> not defined number of args
//for each pointer: cp strings with copyin until NULL

//1- args count
    while(args[args_counter] != NULL){
      args_counter++;
    }
    // implement error: total size of arg strings too large
    if (args_counter > ARG_MAX) {
        return E2BIG;
    }
    
  
    char *progr_copy = (char *) kmalloc(sizeof(char) * PATH_MAX);
    size_t actual = 0;
    res_executable = copyinstr((userptr_t)progr, progr_copy, PATH_MAX, &actual);
    //fail copy instruction  
    if (res_executable) {
        kfree(progr_copy);
        return res_executable;
    }
   //TMP STORE ARGS, too many execv generate lots of kmalloc that break the os

    //kmem to args
    char **args_copy = (char **) kmalloc(sizeof(char *) * args_counter);
    //check if args access to a not allowed addr space
    while(args_copy[args_counter_copy] != NULL){
      if ((int *)args[args_counter_copy] == (int *)0x40000000 || (int *)args[args_counter_copy] == (int *)0x80000000) {
        kfree(progr_copy);
        return EFAULT;
        break; //not needed
      }
      else args_counter_copy++;
    }


    //  PADDING: mips divisible by 4
    //no need to pad also the name because is a char (1byte no allign issues)
    //characters array can start from any addr
    args_counter_copy = 0;
    for (;args_counter_copy < args_counter; args_counter_copy++) {
        asize = strlen(args[args_counter_copy]) + 1; //lenght of word+ termiination
        //allocate strings
        args_copy[args_counter_copy] = (char *) kmalloc(sizeof(char) * asize);
        copyinstr((userptr_t)args[args_counter_copy], args_copy[args_counter_copy], asize, &actual);
        //padding is the sum of every string size+ relative padding
        padding+= asize;
        //if modulo is not =0, aka is not divisible for 4
        int modul=padding % 4;
        if (modul) padding+= (4 - modul) % 4;
    }
//mips: pointers alignment by 4


////////////////////////////////


  //AS RUNPROGRAM: open executable, create new addr space, load elf
  //open exe file
	res_executable = vfs_open(progr, O_RDONLY, 0, &v_exe);
	if (res_executable) {
    kfree(progr_copy);
		return res_executable;
	}

  //NB DESTROY CURRENT ADDR SPACE TO CHECK getas==null
old_as=curproc->p_addrspace;
as_destroy(old_as);




	// We should be a new process

	KASSERT(proc_getas() == NULL);

	// Create a new address spac. 
	new_as = as_create();
	if (new_as == NULL) {
    kfree(progr_copy);
		vfs_close(v_exe);
		return ENOMEM;
	}

	// Switch to it and activate it
	proc_setas(new_as);
	as_activate();

	// Load the executable. 
	res_executable = load_elf(v_exe, &entrypoint);
	if (res_executable) {
		//p_addrspace will go away when curproc is destroyed 
    kfree(progr_copy);
		vfs_close(v_exe); 
		return res_executable;
	}

  //close executable file: done 
  vfs_close(v_exe);
  /////////////////////////////////////////////
  
 //cp args from kern buff to usr space: USE USER STACK space, the only known
  //as_define_stack: user stack, aka USER_SPACE_TOP
  res_k2u = as_define_stack(new_as, &stackptr);
	if (res_k2u) {
		// p_addrspace will go away when curproc is destroyed 
    kfree(progr_copy);
		return res_k2u;
	}

 //stack is 8 bit aligned
  stackptr=-padding; //occupied addr space 
  //take into account termination
  char **args_addr = (char **) kmalloc(sizeof(char *) * args_counter+1);
  //cp arg to stack and store its adrr


  for (int i=0;i< args_counter;i++ ) {
      asize = strlen(args_copy[args_counter]) + 1; //lenght of word+ termiination
      
      //if modulo is not =0, aka is not divisible for 4
      int modul_size=asize % 4;
      if (modul_size) asize+= (4 - modul_size) % 4;
      args_addr[i]=(char*)stackptr;
      copyoutstr(args_copy[i], (userptr_t)stackptr, asize, &actual);
      stackptr+=asize;
  }
  args_addr[args_counter]=0; //termination
  stackptr=-padding; //occupied addr space 
  stackptr=-(args_counter+1)* sizeof(char *); //occupied addr space 


  //cp addr of addrs into stack
   for (int i=0;i< args_counter;i++ ) {
      copyout((args_addr+1), (userptr_t)stackptr, sizeof(char *));
      stackptr+=sizeof(char *);
  }

  // END PROCESS: FREE COPIESS
  //go to the start of the stack
  stackptr=-(args_counter+1)* sizeof(char *); 
  kfree(progr_copy);

  args_counter_copy = 0;
    for (;args_counter_copy < args_counter; args_counter_copy++){
      kfree(args_copy[args_counter_copy]);
    }
  kfree(args_copy);
  kfree(args_addr);
  
   /////////////////////////////////
  //AS RUNPROGRAM: run user mode, use function "enter new proc"
  // Warp to user mode
  //argc,userspace addr of argv,userspace addr of environment...
	enter_new_process(args_counter,(userptr_t) stackptr, (userptr_t)stackptr , 
			  stackptr, entrypoint);

	// enter_new_process does not return
	panic("enter_new_process returned\n");
	return EINVAL;
  
/*
 (void)progr;
 (void)args;
 return 0;
 */
}
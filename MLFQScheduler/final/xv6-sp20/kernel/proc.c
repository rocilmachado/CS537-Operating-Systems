#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  int queue[4][NPROC];             //Global array for MLFQ. Each layer can hold a maximum of NPROC processes
  int priorityLevelCount[4];
} ptable;

int ticksPerPriority[4] = {-1,32,16,8};
int rrticksPerPriority[4] = {64,4,2,1};
//int mlfq[4][NPROC] = {{-1},{-1},{-1},{-1}};
//int priorityLevelCount[4];


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);


void enqueue(int (*pqueue)[NPROC], struct proc* process)
{
  for(int i = 1; i <= NPROC ; i++)
    {
      //if(*pqueue[i] == -1)
      {
        cprintf("%d\n", *pqueue[i]);

      }
    }
  for(int i = 0; i < NPROC ; i++)
    {
      if(*pqueue[i] == -1)
      {
        cprintf("Process added to queue at index %d at priority %d\n", i, process->priority);
        *pqueue[i] = process->pid;
        break;

      }
    }

    for(int i = 1; i <= NPROC ; i++)
    {
      //if(*pqueue[i] == -1)
      {
        //cprintf("%d, ", *pqueue[i]);

      }
    }
    return;
}

void dequeue(int (*pqueue)[NPROC], int pid)
{
  for(int i = 0; i < NPROC ; i++)
  {
    if(*pqueue[i] == pid)
    {
      *pqueue[i] = -1;
      break;
    }
  }

}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
 // ptable.priorityLevelCount[0] = -1;
 // ptable.priorityLevelCount[1] = -1;
 // ptable.priorityLevelCount[2] = -1;
 // ptable.priorityLevelCount[3] = -1;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  //MLFQ : Initialize the number of ticks used per process and number of wait_ticks to 0
  for(int i = 0 ; i < 4 ; i++)
  {
    p->ticks[i] = 0;
    p->wait_ticks[i] = 0;
    p->rrticksleft[i]= rrticksPerPriority[i];
  }
  p->priority = 3;    //Highest priority
  ptable.priorityLevelCount[3]++;
  //cprintf("%d %s inside userinit\n", p->pid, p->name);
  ptable.queue[3][ptable.priorityLevelCount[3]] = p->pid;
  //enqueue(&mlfq[3], p);
  //cprintf("New process added to priority 3 : %d\n", p->pid);

  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  cprintf("Process exited: %d\n", proc->pid);
  //for(int i = 3 ; i >= 0 ; i--)
 // {
	  //cprintf("ticks left in level %d is %d\n", i, proc->wait_ticks[i]);
 // }
 //
 
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//Finds the next process to run out of the
//possible NPROC processes in the queue at the given priority 
int getNextProcToRunInQueue(int priority)
{
  int idx;
  struct proc currproc;
  for(idx = 0; idx < NPROC; idx++)
  {
    currproc = ptable.proc[idx];
    if(currproc.priority == priority)
    {
      if(currproc.state == RUNNABLE)
      {
	//if(currproc->ticks[priority] % rrticksPerPriority[priority]) 

        return idx;
      }
    }

  }

  return -1;

}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  int priority,y;
  int found;
  //struct proc* schedproc;  //PID of the process scheduled to run next
  //int inextproc = -1;
  int index = 0;
  struct proc* process;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
/********************************************************************************************************************************
    for(priority = 3 ; priority >= 0 ; priority--)
    {
      inextproc = getNextProcToRunInQueue(priority);
      if(inextproc != -1)
        {
          schedproc = &ptable.proc[inextproc];
          schedproc->wait_ticks[priority] = 0;
          schedproc->ticks[priority]++;
          if(priority != 0)
            {
              if(schedproc->ticks[priority] % ticksPerPriority[priority] == 0)
              {
                schedproc->priority--;
		            cprintf("Process %d demoted to priority %d\n", schedproc->pid, schedproc->priority);
              }
            }
          break;

        }
    }

    if(inextproc == -1)
    {
      release(&ptable.lock);
      continue;
    }

    if(priority == 0)
    {

    }

    for(index = 0; index < NPROC ; index++)
    {
      process = &ptable.proc[index];
      if(process->state != RUNNABLE)
        continue;
      
      //Update wait_ticks for all the other RUNNABLE processes except the chosen process
      if(index != inextproc)
      {
        process->wait_ticks[process->priority]++;
        if(process->priority !=3)
         {
           int ticksToPromote = (ticksPerPriority > 0)? ticksPerPriority[process->priority]* 10 : 640;
           if(process->wait_ticks[process->priority] == ticksToPromote)
            {
              process->wait_ticks[process->priority] = 0;
              process->priority++;
	      cprintf("Process %d boosted to priority %d\n",process->pid, process->priority); 
              //sys_boostproc();
            }
         }

      }
    }
********************************************************************************************************************************************/

   for(priority = 3; priority >=0; priority--)
   {
      found = 0;
     for(y = 0; y <= ptable.priorityLevelCount[priority]; y++) {
       int processToBeRunPID = ptable.queue[priority][y];
       for(p = ptable.proc ; p < &ptable.proc[NPROC]; p++) 
       {
          if(p->pid == processToBeRunPID)
          {
            //found = 1;
            break;
          }
      }
      if(p->state == RUNNABLE) {
       // cprintf("Process found\n");
	//p->ticks[p->priority]++;
	//p->wait_ticks[p->priority] = 0;
	//p->rrticksleft[p->priority]--;
        found = 1;
        break;
     }
     }
     if(found == 1) {
        break;
     }
   }

   if(found == 0)
    {
      release(&ptable.lock);
      continue;
    }
  //cprintf("Found the process %d\n", p->pid);
  //

  p->ticks[p->priority]++;
  p->wait_ticks[p->priority] = 0;
  p->rrticksleft[p->priority]--;

   if(p->rrticksleft[p->priority] == 0 /*&& p->priority != 3*/)
   {
     int a;
     for(a = 0; a < ptable.priorityLevelCount[p->priority]; a++) {
             if(ptable.queue[p->priority][a] == p->pid) {
		    // cprintf("Process found at a = %d\n", a);
                 break;
	     }
       }
       for(int z = a; z < ptable.priorityLevelCount[p->priority]; z++) {
	      ptable.queue[p->priority][z] = ptable.queue[p->priority][z+1];
       }
       ptable.queue[p->priority][ptable.priorityLevelCount[p->priority]-1] = p->pid;
       //cprintf("process %d moved to the end of queue %d after %d ticks\n", p->pid, p->priority, p->ticks[p->priority]);
       //p->qtail[priority] = p->qtail[priority] + 1;
       p->rrticksleft[p->priority] = rrticksPerPriority[p->priority];
   }


   if(p->priority > 0)
    {
      if(p->ticks[p->priority] % ticksPerPriority[p->priority] == 0)
      {
        int a;
        for(a = 0; a < ptable.priorityLevelCount[p->priority]; a++) {
             if(ptable.queue[p->priority][a] == p->pid) {
		     //cprintf("Process %d found at a = %d\n", p->pid, a);
                 break;
	     }
       }
       for(int z = a; z < ptable.priorityLevelCount[p->priority]; z++) {
	      ptable.queue[p->priority][z] = ptable.queue[p->priority][z+1];
       }
        p->rrticksleft[p->priority] = rrticksPerPriority[p->priority];
       // ptable.queue[p->priority][ptable.priorityLevelCount[p->priority]-1] = -1;
        ptable.priorityLevelCount[p->priority]--;
        p->priority--;
        ptable.queue[p->priority][ptable.priorityLevelCount[p->priority]] = p->pid;
        ptable.priorityLevelCount[p->priority]++;
        p->rrticksleft[p->priority] = rrticksPerPriority[p->priority];
		    //cprintf("Process %d demoted to priority %d\n", p->pid, p->priority);
      }
    }


    for(index = 0; index < NPROC ; index++)
    {
      process = &ptable.proc[index];
      if(process->state != RUNNABLE)
        continue;
      
      //Update wait_ticks for all the other RUNNABLE processes except the chosen process
      if(process->pid != p->pid)
      {
        process->wait_ticks[process->priority]++;
        if(process->priority < 3)
         {
           int ticksToPromote = (ticksPerPriority > 0)? ticksPerPriority[process->priority]* 10 : 640;
           if(process->wait_ticks[process->priority] == ticksToPromote)
            {
              int a;
              for(a = 0; a < ptable.priorityLevelCount[process->priority]; a++) {
              if(ptable.queue[process->priority][a] == process->pid) {
                // cprintf("Process %d found at a = %d\n",process->pid, a);
		            break;
	            }
              }
              for(int z = a; z < ptable.priorityLevelCount[process->priority]; z++) {
	              ptable.queue[process->priority][z] = ptable.queue[process->priority][z+1];
              }
              //ptable.queue[p->priority][ptable.priorityLevelCount[p->priority]-1] = -1;
              ptable.priorityLevelCount[process->priority]--;
              process->wait_ticks[process->priority] = 0;
              process->rrticksleft[process->priority] = rrticksPerPriority[process->priority];
              process->priority++;
              ptable.queue[process->priority][ptable.priorityLevelCount[process->priority]] = process->pid;
              ptable.priorityLevelCount[process->priority]++;
              process->rrticksleft[process->priority] = rrticksPerPriority[process->priority];
	           // cprintf("Process %d boosted to priority %d\n",process->pid, process->priority); 
              //sys_boostproc();
            }
         }

      }
   
    }

/*  ORIGINAL XV6 SCHEDULER CODE
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }   */

  
    //proc = schedproc;
    //cprintf("Process scheduled is : %d", proc->pid);
    proc = p;
    switchuvm(proc);
    p->state = RUNNING;
    swtch (&cpu->scheduler, proc->context);
    switchkvm();
    proc = 0;

    release(&ptable.lock);
  }

}
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int getprocinfo(struct pstat* procinfo)
{
	if(procinfo == NULL)
		return -1;
	int i = 0;
	struct proc* p;
  acquire(&ptable.lock);

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	{
		procinfo->pid[i] = p->pid;

		procinfo->priority[i] = p->priority;

		
    if(p->state == UNUSED || p->state == EMBRYO || p->state == ZOMBIE)
		{
			procinfo->inuse[i] = 0;
		}
		else
		{
			procinfo->inuse[i] = 1;
		}
    
    //procinfo->inuse[i] = (procinfo->state == UNUSED? 0 : 1);

		procinfo->state[i] = p->state;

		for(int pri = 0; pri < 4 ; pri++)
    {
			procinfo->ticks[i][pri] = p->ticks[pri];
      procinfo->wait_ticks[i][pri] = p->wait_ticks[pri];
    }

		i++;
	}

  release(&ptable.lock);

	return 0;

}
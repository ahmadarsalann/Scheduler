/*****************************************************************
 * *       proc.c - simplified for CPSC405 Lab by Gusty Cooper, University of Mary Washington
 * *       adapted from MIT xv6 by Zhiyi Huang, hzy@cs.otago.ac.nz, University of Otago
 * ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "defs.h"
#include "proc.h"
#include <stdbool.h>

static void wakeup1(int chan);

/* Dummy lock routines. Not needed for lab
 * In the real Xv6 OS, acquire(&ptable.lock) get the lock associated with the table of processes. 
 * The table of processes is a critical region, which must be locked - just like we are studying. 
 * In our program, the call does not do anything because we do not have critical regions. 
 * In the real Xv6 code, acquire() disables interrupts.
 * 			- From the wise words of gusty 
 */

// Dummy lock routines. Not needed for lab

void acquire(int *p) {
	return;
}

void release(int *p) {
	return;
}

// enum procstate for printing
char *procstatep[] = { "UNUSED", "EMPRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE" };

// Table of all processes
struct {
	int lock;   // not used in Lab
	struct proc proc[NPROC];
} ptable;

// Initial process - ascendent of all other processes
static struct proc *initproc;

// Used to allocate process ids - initproc is 1, others are incremented
int nextpid = 1;

// Funtion to use as address of proc's PC
void forkret(void){

}

// Funtion to use as address of proc's LR
void trapret(void){

}

// Initialize the process table
void pinit(void){
	memset(&ptable, 0, sizeof(ptable));
}

// Look in the process table for a process id
// If found, return pointer to proc
// Otherwise return 0.
static struct proc* findproc(int pid){

	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid == pid){
			return p;
		}
	}
	return 0;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc* allocproc(void){
	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->state == UNUSED){
			goto found;
		}
	}
	return 0;

found:
	p->state = EMBRYO;
	p->pid = nextpid++;

	p->context = (struct context*)malloc(sizeof(struct context));
	memset(p->context, 0, sizeof *p->context);
	p->context->pc = (uint)forkret;
	p->context->lr = (uint)trapret;

	return p;
}

// Set up first user process.
int userinit(void){
	struct proc *p;
	p = allocproc();
	initproc = p;
	p->sz = PGSIZE;
	strcpy(p->cwd, "/");
	strcpy(p->name, "userinit"); 
	p->state = RUNNING;
	curr_proc = p;
	return p->pid;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int Fork(int fork_proc_id){
	int pid;
	struct proc *np, *fork_proc;

	// Find current proc
	if ((fork_proc = findproc(fork_proc_id)) == 0)
		return -1;

	// Allocate process.
	if((np = allocproc()) == 0)
		return -1;

	// Copy process state from p.
	np->sz = fork_proc->sz;
	np->parent = fork_proc;
	// Copy files in real code
	strcpy(np->cwd, fork_proc->cwd);

	pid = np->pid;
	np->state = RUNNABLE;
	strcpy(np->name, fork_proc->name);
	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
int Exit(int exit_proc_id){
	struct proc *p, *exit_proc;

	// Find current proc
	if ((exit_proc = findproc(exit_proc_id)) == 0)
		return -2;

	if(exit_proc == initproc) {
		printf("initproc exiting\n");
		return -1;
	}

	// Close all open files of exit_proc in real code.

	acquire(&ptable.lock);

	wakeup1(exit_proc->parent->pid);

	// Place abandoned children in ZOMBIE state - HERE
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->parent == exit_proc){
			p->parent = initproc;
			p->state = ZOMBIE;
		}
	}

	exit_proc->state = ZOMBIE;

	// sched();
	release(&ptable.lock);
	return 0;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
// Return -2 has children, but not zombie - must keep waiting
// Return -3 if wait_proc_id is not found
int Wait(int wait_proc_id){
	struct proc *p, *wait_proc;
	int havekids, pid;

	// Find current proc
	if ((wait_proc = findproc(wait_proc_id)) == 0)
		return -3;

	acquire(&ptable.lock);
	for(;;){ // remove outer loop
		// Scan through table looking for zombie children.
		havekids = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->parent != wait_proc)
				continue;
			havekids = 1;
			if(p->state == ZOMBIE){
				// Found one.
				pid = p->pid;
				p->kstack = 0;
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
		if(!havekids || wait_proc->killed){
			release(&ptable.lock);
			return -1;
		}
		if (havekids) { // children still running
			Sleep(wait_proc_id, wait_proc_id);
			release(&ptable.lock);
			return -2;
		}

	}
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
int Sleep(int sleep_proc_id, int chan){
	struct proc *sleep_proc;
	// Find current proc
	if ((sleep_proc = findproc(sleep_proc_id)) == 0)
		return -3;

	sleep_proc->chan = chan;
	sleep_proc->state = SLEEPING;
	return sleep_proc_id;
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(int chan){
	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->state == SLEEPING && p->chan == chan)
			p->state = RUNNABLE;
}


void Wakeup(int chan){
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}



// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int Kill(int pid){
	struct proc *p;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid == pid){
			p->killed = 1;
			// Wake process from sleep if necessary.
			if(p->state == SLEEPING)
				p->state = RUNNABLE;
			return 0;
		}
	}
	release(&ptable.lock);
	return -1;
}


// Getting the number of procs in the program.
int n_proc(){
	struct proc *p;
	int nproc = 0;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid > 0){
			nproc++;
		}//end of if
	}//end of for
	return nproc;
}
bool check(int pid){
	struct proc *p;
	p = findproc(pid);
	if(p->state == UNUSED || p->state == EMBRYO || p->state == SLEEPING || p->state == ZOMBIE){
		return false;
	}else{
		return true;
	}
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void){
	//A continous loop in real code
	//  if(first_sched) first_sched = 0;
	//  else sti();

	// get a random number for the lottery
	int random = (rand() % 100) + 1; //to get the random number
	printf("The random number is %d\n", random);//print the random number

	// number of procs in the program	
	int number = n_proc();//to get the total active procs
	// divide that number from 100
	int base = 100 / number;//get the base number

	char names[100][200];//names array to store strings
	int priority[100];//initialize the priority array

	struct proc *p;//to get the proc

	acquire(&ptable.lock);//to get the lock
	// pause current running process

	int y = 0;//initlize y to zero
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//loop for iteration
		if(p->pid > 0){//if the pid is greater than zero than do this
			strcpy(names[p->pid], procstatep[p->state]);//copy the states to names array
			if(p->pid == 1){//if the pid is one
				priority[p->pid] = 100 / number;//make the priority the base
			}//end of if
			else{
				priority[p->pid] = base * p->pid;//else multiple the base by the pid and make that the new priority.
			}//end of else
			y++;//increment y
		}//end of if
	}//end of for

	curr_proc->state = RUNNABLE;//make the curr proc state RUNNABLE
	struct proc *l;//declare a second proc


	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//make a loop that iterates over this
		int n = p->pid;//give n the pid
		if(priority[n] >= random){//if the priority is greater than random value
			if(check(n) == true){//check if the proc is good
				curr_proc = p;//set current proc to p
				p->state = RUNNING;//Make it running
				break;//go out
			}else{
				l = findproc(n+1);//else find the next pid
				curr_proc = l;//set it to current proc
				l->state = RUNNING;//make it RUN
				break;//go out
			}//end of else
		}//end of if
		if(priority[n] < random && random < priority[n+1]){//if the priority is less than random and random is less than the second process priority 
			if(check(n) == true && check(n+1) == true){//check if the procs are good
				l = findproc(n+1);//find the next proc
				curr_proc = l;//set it to current proc
				l->state = RUNNING;//change state to RUNNING
				break;//go out
			}//end of if
			else{
				l = findproc(n-1);//else find the proc before
				curr_proc = l;//set it to current proc
				l->state = RUNNING;//set the state to running
				break;//go out
			}//end of else
		}//end of if
		else{//else
			continue;//iterate again
		}//end of else
	}//end of for

	for(int i = 1; i <= y; i++){//loop
		printf("The values in the array are: %d\n", priority[i]);//print the priority array for testing
	}//end of for

	release(&ptable.lock);//release the lock
}//end of shceduler

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void){
	struct proc *p;

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid > 0){
			printf("pid: %d, parent: %d state: %s\n", p->pid, p->parent == 0 ? 0 : p->parent->pid, procstatep[p->state]);
		}//end of if
	}//end of for
}

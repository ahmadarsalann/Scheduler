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
#include <math.h>
#include <stdbool.h>

/*
   static const int priority_to_weight[40] = {
// -20 //  88761, 71755, 56483, 46273, 36291,
// -15 //  29154, 23254, 18705, 14949, 11916,
// -10 //  9548, 7620, 6100, 4904, 3906,
// -5  //  3121, 2501, 1991, 1586, 1277,
//  0  //  1024, 820, 655, 526, 423,
//  5  //  335, 272, 215, 172, 137,
// 10  // 110, 87, 70, 56, 45,
// 15  // 36, 29, 23, 18, 15
};
*/


static void wakeup1(int chan);

// Dummy lock routines. Not needed for lab
void acquire(int *p) {
	return;
}

void release(int *p) {
	return;
}

// enum procstate for printing
char *procstatep[] = { "UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE" };

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

	// return the pic
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

static int fweight = 0;//to get the total weight of all processes
void timeslice(int pid){//to get the time slice
	int min = 6;//the min granularity.
	struct proc *p; //declare a proc to use
	p = findproc(pid);//to find the proc and make the p that proc
	float ts = ((float)p->weight/(float)fweight) * 48;//use advance calculation to get the timeslice
	if(ts < min){//if timeslice is less than the min granularity
		ts = min;//equal timeslice to min granularity
	}//end of if
	p->time_slice = ts;//make the proc point timeslice to its struct variable
}//end of timeslice


static double vrun[100];//to put all the vruntimes in the array
void vruntime(int pid){//the vruntime function to get the vruntime
	struct proc *p;//declare a proc
	p = findproc(pid);//use that proc to find the proc
	float ts = p->time_slice;//store the time slice of the proc in a float
	float procweight = p->weight;//get the weight of the proc and store in a float
	float vtime = p->vruntime;//get the vtime to store additional vtime
	float final = vtime + ((float)1024.00/procweight * ts);//do calculation to get the final vtime
	vrun[p->pid] = final;//put the vtime in the array
	p->vruntime = final;//make the struct proc point at the vtime
}//end of vruntime

bool check(int pid){//function to the check if any active proc are anything but RUNNABLE or RUNNING
	struct proc *p;//declare a proc
	p = findproc(pid);//we find the proc and put it in p
	if(p->state == UNUSED || p->state == EMBRYO || p->state == SLEEPING || p->state == ZOMBIE){//if the proc are any of these states
		return false;//return false
	}//end of if
	return true;//otherwise return true
}//end of check

//to assign nice value to a process
void assignnice(int pid, int nice){//to assign the value to a proc
	struct proc *p;//declare a struct proc
	p = findproc(pid);//use the pid to find the proc and equal p to that proc
	p->nice_value = nice;//use the nice and assign a nice value to the proc
}//end of assignnice

void assignweight(int pid){//to assign the weight
	struct proc *p;//to declare the proc 
	p = findproc(pid);//use the pid and find the proc
	float down = pow(1.25, p->nice_value);//use pow to do some math
	float weight = 1024/down;//divide down by 1024 to get the weight
	int weight1;//get an int
	weight1 = weight;//equal float to int
	p->weight = weight;//assign int weight1 to proc
	fweight = fweight + weight1;//add the weight that holds weight of all procs
}//end of assignweight


// Gets which procs are active.                                                                     
int n_proc(){//to get the number of procs that are active
	struct proc *p;//declares the proc
	int nproc = 0;//initialize nproc to 0

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//for loop to get the number of procs
		if(p->pid > 0){//if pid is greater than zerp
			nproc++;//add it to the nproc variable
		}//end of if
	}//end of for
	return nproc;//return the number of procs
}

/* Completely Fair Scheduler:
 * 	the goal is to fairly divide a CPU evenly among all competing
 * 	processes through virtual runtime. As each process runs, it 
 * 	accumulates virtual runtime. In this case p->virtual_runtime. 
 * 	[*] The CFS will pick the process with the LOWEST runtime!
 */
void scheduler(void){
	//A continous loop in real code
	//  if(first_sched) first_sched = 0;
	//  else sti();
	curr_proc->state = RUNNABLE;//make the current proc RUNNABALE
	if(fweight > 0){//when we use this function and if the total weight is not zero
		fweight = 0;//make it zero
	}//end of if
	
	
	int nice;//declare int nice
	char answer[20];//declare answer array
	struct proc *p;//declare proc
	acquire(&ptable.lock);//acquire the lock
	printf("Hey User, will you input the nice value of all processes? Y/N \n");//prints the statement
	scanf("%s", answer);//get the input from user and stores it
	printf("\n");//new line
	getchar();//gets rid of the last character in scanf buffer
	if(strcmp(answer, "Y") == 0){//if the answer is yes than do this
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//for loop to iterate over the procs
			if(p->pid > 0 && check(p->pid)){//if the p->pid is greater than zero and all the Procs are RUNNABLE or RUNNING then do this
				printf("Enter the nice value of p->pid: %d\n: ", p->pid);//enter the nice value of all active procs
				scanf("%d", &nice);//get the input from user
				if(nice >= -20 && nice <= 20){//if the input is between these two numbers
					printf("\n");//then print a new line
					getchar();//get the last character in stdin buffer and removes it
					assignnice(p->pid, nice);//call the assign nice function
					assignweight(p->pid);//call the assign weight function
				}//end of if
				else{
					printf("Wrong nice value!\n");//if the user did not input a correct nice value then print this
					p--;//decrement p
				}//end of else
			}//end of if
		}//end of for
	}else{
		printf("Then I cant schedule processes! \n");//if the user did not enter "Y" then print this
	}//end of else
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//make a loop to iterate
		if(p->pid > 0){//if the pid is greater than zero
			timeslice(p->pid);//call timeslice function
			vruntime(p->pid);//call vtime function
		}//end of if
	}//end of for
	int size = 0;//declare the size to 0
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//to iterate over the procs
		if(p->pid > 0){//if pid greater than zero
			size++;//increment size
		}//end of if
	}//end of for

	int small = 1;//declare small to 1
	for(int i = 1; i <= size ; i++){//make a loop to iterate over
		if(vrun[i] < vrun[small]){//if statement to get the smallest vtime
			small = i;//equal the small to i
		}//end of if
	}//end of for
	

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){//loop to do final things
		if(p->vruntime == vrun[small]){//get the smallest vtime
			curr_proc = p;//start scheduling
			p->state = RUNNING;//change the state
			break;//go out
		}//end of if
	}//end of for

	release(&ptable.lock);//release the lock

}//end of scheduler



// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void){
	struct proc *p;
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->pid > 0)
			printf("pid: %d, parent: %d state: %s nice: %d weight: %d time_slice: %f vruntime: %.10f\n", p->pid, p->parent == 0 ? 0 : p->parent->pid, procstatep[p->state], p->nice_value, p->weight, p->time_slice, p->vruntime);
}

#include <cstring>
#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

/*
This program does the following.
1) Create handlers for two signals.
2) Create an idle process which will be executed when there is nothing
   else to do.
3) Create a send_signals process that sends a SIGALRM every so often.

If compiled with -DEBUG, when run it should produce the following
output (approximately):

$ ./a.out
state:        3
name:         IDLE
pid:          21145
ppid:         21143
interrupts:   0
switches:     0
started:      0
in CPU.cc at 240 at beginning of send_signals getpid() = 21144
in CPU.cc at 245 sending signal = 14
in CPU.cc at 246 to pid = 21143
In ISR stopped:     21145
---- entering scheduler
continuing    21145
---- leaving scheduler
in CPU.cc at 245 sending signal = 14
in CPU.cc at 246 to pid = 21143
In ISR stopped:     21145
---- entering scheduler
continuing    21145
---- leaving scheduler
in CPU.cc at 245 sending signal = 14
in CPU.cc at 246 to pid = 21143
In ISR stopped:     21145
---- entering scheduler
continuing    21145
---- leaving scheduler
in CPU.cc at 245 sending signal = 14
in CPU.cc at 246 to pid = 21143
In ISR stopped:     21145
---- entering scheduler
continuing    21145
---- leaving scheduler
in CPU.cc at 245 sending signal = 14
in CPU.cc at 246 to pid = 21143
in CPU.cc at 250 at end of send_signals
In ISR stopped:     21145
---- entering scheduler
continuing    21145
Terminated: 15
---------------------------------------------------------------------------
Add the following functionality.
1) Change the NUM_SECONDS to 20.
	#define NUM_SECONDS 20

2) Take any number of arguments for executables and place each on the
   processes list with a state of NEW. The executables will not require
   arguments themselves.
	-What exactly are we passing in as arguments? PID#s?
	for (int i = 0; i < argc; i++) {
		processes.add(argv[i + 1]);
		processes[i].STATE = NEW;
	}

3) When a SIGALRM arrives, scheduler() will be called; it currently simply restarts the idle process. Instead, do the following.
	ISV[SIGALARM] = scheduler;
	ISV[SIGCHLD] = process_done;
	
   a) Update the PCB for the process that was interrupted including the
      number of context switches and interrupts it had and changing its
      state from RUNNING to READY.
	-How do i know which process was interrupted?
	-How do I find the number of context switches and interrupts it had?
	tocont->state = READY;
	tocont->name = "NAME";   // name of the executable
	tocont->pid = getpid();  // process id from fork();
	tocont->ppid = getpid(); // parent process id
	// tocont->interrupts;      // number of times interrupted
	// tocont->switches;        // may be < interrupts
	// tocont->started;         // the time this process started

   b) If there are any NEW processes on processes list, change its state to RUNNING, and fork() and execl() it.
	for each in processes {
		if (processes->status == NEW) {
			processes->status = RUNNING;
			int pid = fork();
			if (pid == 0) {		// child
			// if((processes->pid = fork()) == 0) {
				execl();			
			}
			else {			// parent
				continue;
			}		
		}
		else {
					
		}
	}

   c) If there are no NEW processes, round robin the processes in the
      processes queue that are READY. If no process is READY in the
      list, execute the idle process.
	-Round Robin, then execute?

4) When a SIGCHLD arrives notifying that a child has exited, process_done() is called. process_done() currently only prints out the PID and the status.
   a) Add the printing of the information in the PCB including the number
      of times it was interrupted, the number of times it was context
      switched (this may be fewer than the interrupts if a process
      becomes the only non-idle process in the ready queue), and the     
      total system time the process took.
   b) Change the state to TERMINATED.
   c) Restart the idle process to use the rest of the time slice.
	running = idle; 	// ? 
*/

#define NUM_SECONDS 20
#define EVER ;;

#define assertsyscall(x, y) if(!((x) y)){int err = errno; \
    fprintf(stderr, "In file %s at line %d: ", __FILE__, __LINE__); \
        perror(#x); exit(err);}

#ifdef EBUG
#   define dmess(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << endl;

#   define dprint(a) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << (#a) << " = " << a << endl;

#   define dprintt(a,b) cout << "in " << __FILE__ << \
    " at " << __LINE__ << " " << a << " " << (#b) << " = " \
    << b << endl
#else
#   define dmess(a)
#   define dprint(a)
#   define dprintt(a,b)
#endif

using namespace std;

// http://man7.org/linux/man-pages/man7/signal-safety.7.html

#define WRITES(a) { const char *foo = a; write(1, foo, strlen(foo)); }
#define WRITEI(a) { char buf[10]; assert(eye2eh(a, buf, 10, 10) != -1); WRITES(buf); }

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };

struct PCB
{
	STATE state;
	const char *name;   // name of the executable
	int pid;            // process id from fork();
	int ppid;           // parent process id
	int interrupts;     // number of times interrupted
	int switches;       // may be < interrupts
	int started;        // the time this process started
};

PCB *process;
PCB *running;
PCB *idle;

// http://www.cplusplus.com/reference/list/list/
// list<PCB *> new_list;
list<PCB *> processes;

int sys_time;

/*
** Async-safe integer to a string. i is assumed to be positive. The number
** of characters converted is returned; -1 will be returned if bufsize is
** less than one or if the string isn't long enough to hold the entire
** number. Numbers are right justified. The base must be between 2 and 16;
** otherwise the string is filled with spaces and -1 is returned.
*/
int eye2eh(int i, char *buf, int bufsize, int base)
{
	if(bufsize < 1) {return(-1);}
	buf[bufsize-1] = '\0';
	if(bufsize == 1) {return(0);}
	if(base < 2 || base > 16)
	{
		for(int j = bufsize-2; j >= 0; j--)
		{
			buf[j] = ' ';
		}
		return(-1);
	}

	int count = 0;
	const char *digits = "0123456789ABCDEF";
	for(int j = bufsize-2; j >= 0; j--)
	{
		if(i == 0)
		{
			buf[j] = ' ';
		}
		else
		{
			buf[j] = digits[i%base];
			i = i/base;
			count++;
		}
	}
	if(i != 0) {return(-1);}
	return(count);
}

/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab(int signum) { WRITEI(signum); WRITES("\n"); }

// c++decl> declare ISV as array 32 of pointer to function(int) returning void
void(*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR(int signum)
{
	if(signum != SIGCHLD)
	{
		running->interrupts++;
		assertsyscall (kill (running->pid, SIGSTOP), != -1);

		WRITES("In ISR stopped: ");
		WRITEI(running->pid);
		WRITES("\n");
		running->state = READY;
	}

	ISV[signum](signum);
}

/*
** an overloaded output operator that prints a PCB
*/
ostream& operator <<(ostream &os, struct PCB *pcb)
{
	os << "state:        " << pcb->state << endl;
	os << "name:         " << pcb->name << endl;
	os << "pid:          " << pcb->pid << endl;
	os << "ppid:         " << pcb->ppid << endl;
	os << "interrupts:   " << pcb->interrupts << endl;
	os << "switches:     " << pcb->switches << endl;
	os << "started:      " << pcb->started << endl;
	return(os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator <<(ostream &os, list<PCB *> which)
{
	list<PCB *>::iterator PCB_iter;
	for(PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++)
	{
		os <<(*PCB_iter);
	}
	return(os);
}

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals(int signal, int pid, int interval, int number)
{
	dprintt("at beginning of send_signals", getpid());

	for(int i = 1; i <= number; i++)
	{
		assertsyscall(sleep(interval), == 0);
		dprintt("sending", signal);
		dprintt("to", pid);
		assertsyscall(kill(pid, signal), == 0)
	}

	dmess("at end of send_signals");
}

struct sigaction *create_handler(int signum, void(*handler)(int))
{
	struct sigaction *action = new(struct sigaction);

	action->sa_handler = handler;

/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop(i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
	if(signum == SIGCHLD)
	{
		action->sa_flags = SA_NOCLDSTOP | SA_RESTART;
	}
	else
	{
		action->sa_flags =  SA_RESTART;
	}

	sigemptyset(&(action->sa_mask));
	assert(sigaction(signum, action, NULL) == 0);
	return(action);
}

void scheduler(int signum)
{
   	WRITES("---- entering scheduler\n");
    	assert(signum == SIGALRM);
    	sys_time++;

    	bool found_one = false;
	unsigned int i;	

	for (i = 0; i < processes.size(); i++)
	{
		PCB *front = processes.front();
		processes.pop_front();
		processes.push_back(front);

		// execute new process and populate its PCB
		if (front->state == NEW)
		{
			WRITES("running NEW process \n");
			front->state = RUNNING;
			front->ppid = getpid();
			front->interrupts = 0;
			front->switches = 0;
			front->started = sys_time;
			running = front;

			if ((front->pid = fork()) == 0)
			{
				// execl(front->name, front->name, NULL), -1);				
				// assert (execl(front->name, front->name, NULL) > 0);			
				assertsyscall (execl (front->name, front->name, NULL), != -1);
			}
			found_one = true;
			break;			
		}
		else if (front->state == READY)
		{
			WRITES("continuing READY process\n");
			if (running->pid != front->pid)
			{
				front->switches++;
				// running->switches++;	
			}
			front->state = RUNNING;
			running = front;
			assertsyscall (kill (front->pid, SIGCONT), != -1);
			
			found_one = true;
			break;
		}
		if (!found_one)
		{
			idle->state = RUNNING;
			running = idle;
			assertsyscall (kill (idle->pid, SIGCONT), != -1);
		}		
	
	}

	WRITES("continuing ");
	WRITEI(running->pid);
	WRITES("\n");
	// cout << running;
	WRITES("---- leaving scheduler\n");


}

void process_done(int signum)
{
	assert(signum == SIGCHLD);
	WRITES("---- entering process_done\n");
	unsigned int i;

	// might have multiple children done.
	for(i = 0; i < processes.size(); i++)
	{
		int status, cpid;

		// we know we received a SIGCHLD so don't wait.
		cpid = waitpid(-1, &status, WNOHANG);

		if(cpid < 0)
		{
		    	WRITES("cpid < 0\n");
		    	assertsyscall(kill(0, SIGTERM), != 0);
		}
		else if(cpid == 0)
		{
		    	// no more children.
		    	break;
		}
		else
		{
			WRITES("process exited: ");
			WRITEI (running->pid);
			WRITES("\n");
			list<PCB *>::iterator PCB_iter;
			for (PCB_iter = processes.begin();
			     PCB_iter != processes.end();
			     PCB_iter++)
			{
				if((*PCB_iter)->pid == cpid) 
				{
					(*PCB_iter)->state = TERMINATED;				
					cout << (*PCB_iter);
					WRITES ("total system time:     ");
					WRITEI (sys_time - (*PCB_iter)->started);
					WRITES ("\n");
					

				}
			}
		}
	}
	// end loop

	running = idle;
	idle->state = RUNNING;
	if (kill (idle->pid, SIGCONT) == -1)
	{
		kill(0, SIGTERM);
	}

	WRITES("---- leaving process_done\n");
}


/*
** set up the "hardware"
*/
void boot()
{
	sys_time = 0;

	ISV[SIGALRM] = scheduler;
	ISV[SIGCHLD] = process_done;
	struct sigaction *alarm = create_handler(SIGALRM, ISR);
	struct sigaction *child = create_handler(SIGCHLD, ISR);

	// start up clock interrupt
	int ret;
	if((ret = fork()) == 0)
	{
		send_signals(SIGALRM, getppid(), 1, NUM_SECONDS);

		// once that's done, cleanup and really kill everything...
		delete(alarm);
		delete(child);
		delete(idle);
		kill(0, SIGTERM);
	}

	if(ret < 0)
	{
		perror("fork");
	}
}

void create_idle()
{
	idle = new(PCB);
	idle->state = READY;
	idle->name = "IDLE";
	idle->ppid = getpid();
	idle->interrupts = 0;
	idle->switches = 0;
	idle->started = sys_time;

	if((idle->pid = fork()) == 0)
	{
		pause();
		perror("pause in create_idle");
	}
}

int main(int argc, char* argv[])
{
	boot();

	// create idle process and set to running
	create_idle();
	running = idle;
	cout << running;

	// add each program argument to the process list
	// we do not execute or fork here, we leave that to the scheduler
	// we do not set pid because these are not running processes yet
	for (int i = 1; i < argc; i++) 
	{
		process = new(PCB);
		process->state = NEW;
		process->name = argv[i];
		process->interrupts = 0;
		process->switches = 0;
		process->started = 0;
		processes.push_back(process);
	}

	cout << processes;

	// we keep this process around so that the children don't die and
	// to keep the IRQs in place.
	for(EVER)
	{
		// "Upon termination of a signal handler started during a
		// pause(), the pause() call will return."
		pause();
	}
	// delete(processes);
}

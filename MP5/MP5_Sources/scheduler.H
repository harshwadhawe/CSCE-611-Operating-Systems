/* 
    Author: R. Bettati, Joshua Capehart
            Department of Computer Science
            Texas A&M University
			
	    A thread scheduler.

*/
#ifndef SCHEDULER_H
#define SCHEDULER_H

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "thread.H"
#include "interrupts.H"
/*--------------------------------------------------------------------------*/
/* !!! IMPLEMENTATION HINT !!! */
/*--------------------------------------------------------------------------*/
/*
    One way to proceed is to implement the FIFO scheduling policy inside
    class 'Scheduler'. 

    If you plan to implement a Round-Robin Scheduler, derive it from class
    'Scheduler', say as class 'RRScheduler'. The class 'RRScheduler' is 
    really just a FIFO scheduler with THREE MODIFICATIONS:
    1. It manages a timer, which fires at the end-of-quantum (EOQ). 
    (For details on how to set up a timer and how to handle timer interrupts 
    see the 1-second timer in 'kernel.C'.)  The timer is set up in the
    constructor.
    2. It uses an additional function, the EOQ handler. This function gets
    called whenever an EOQ timer event fires. The EOQ handler forces the 
    current thread to call the scheduler's 'yield' function.
    3. The 'yield' function must be modified to account for unused quantum
    time. If a thread voluntarily yields, the EOQ timer must be reset in order
    to not penalize the next thread.
 
    (Note that this qualifies as programming at about the level of a baboon.
     Much better woudl be to have the abstract class 'Scheduler' implement 
     the basic scheduling MECHANISMS and provide abstract funtions to define
     the queue management POLICIES in derived classes, 
     such as 'FIFOScheduler'.)
    
 */

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURE - QUEUE */
/*--------------------------------------------------------------------------*/

class Queue
{
private:
	Thread* thread;   // Pointer to the thread at the head of the queue
	Queue* next;      // Pointer to the next node in the queue

public:
	// Default constructor — initializes an empty queue
	Queue()
	{
		thread = nullptr;
		next   = nullptr;
	}

	// Constructor — creates a queue node with the given thread
	Queue(Thread* new_thread)
	{
		thread = new_thread;
		next   = nullptr;
	}

	// Enqueue — add a thread to the end of the queue
	void enqueue(Thread* new_thread)
	{
		// Case: queue is empty
		if (thread == nullptr)
		{
			thread = new_thread;
		}
		else
		{
			// Traverse until the last node and append the new thread
			if (next != nullptr)
			{
				next->enqueue(new_thread);
			}
			else
			{
				// Reached the end — attach new node
				next = new Queue(new_thread);
			}
		}
	}

	// Dequeue — remove and return the thread at the head of the queue
	Thread* dequeue()
	{
		// Case: queue is empty
		if (thread == nullptr)
		{
			return nullptr;
		}

		// Get the thread at the front
		Thread* top = thread;

		// Case: only one node in the queue
		if (next == nullptr)
		{
			thread = nullptr;
		}
		else
		{
			// Move head pointer forward
			thread = next->thread;

			// Store and delete the old next node
			Queue* del_node = next;
			next = next->next;
			delete del_node;
		}

		return top;
	}
};


/*--------------------------------------------------------------------------*/
/* SCHEDULER */
/*--------------------------------------------------------------------------*/

class Scheduler {

  /* The scheduler may need private members... */
   Queue ready_queue;
   int qsize;
   
public:

   Scheduler();
   /* Setup the scheduler. This sets up the ready queue, for example.
      If the scheduler implements some sort of round-robin scheme, then the 
      end_of_quantum handler is installed in the constructor as well. */

   /* NOTE: We are making all functions virtual. This may come in handy when
            you want to derive RRScheduler from this class. */
  
   virtual void yield();
   /* Called by the currently running thread in order to give up the CPU. 
      The scheduler selects the next thread from the ready queue to load onto 
      the CPU, and calls the dispatcher function defined in 'Thread.H' to
      do the context switch. */

   virtual void resume(Thread * _thread);
   /* Add the given thread to the ready queue of the scheduler. This is called
      for threads that were waiting for an event to happen, or that have 
      to give up the CPU in response to a preemption. */

   virtual void add(Thread * _thread);
   /* Make the given thread runnable by the scheduler. This function is called
      after thread creation. Depending on implementation, this function may 
      just add the thread to the ready queue, using 'resume'. */

   virtual void terminate(Thread * _thread);
   /* Remove the given thread from the scheduler in preparation for destruction
      of the thread. 
      Graciously handle the case where the thread wants to terminate itself.*/
  
};
	
	
/*--------------------------------------------------------------------------*/
/* ROUND ROBIN SCHEDULER                                                    */
/*--------------------------------------------------------------------------*/
/* Commit: Initial implementation of RRScheduler class
   - Defines Round-Robin scheduler structure
   - Includes ready queue, tick tracking, and interrupt handling
   - Declares core scheduling operations (yield, resume, add, terminate)
   - Handles end-of-quantum via interrupt
*/
/*--------------------------------------------------------------------------*/

class RRScheduler: public Scheduler, public InterruptHandler
{
	Queue ready_rr_queue;				// Ready queue for Round-Robin scheduler
	int rr_qsize;						// Robin-Robin ready queue size 
	int ticks;							// Number of ticks since last update
	int hz;								// Frequency of update of ticks
	
	void set_frequency( int _hz );		// Set the interrupt frequency for the timer
	
public:
	RRScheduler();
	/*	Setup the Round-Robin scheduler. This sets up the round robin ready queue.
		The end_of_quantum handler is registered. */
	
	virtual void yield();
	/* Called by the currently running thread in order to give up the CPU. 
      The scheduler selects the next thread from the ready queue to load onto 
      the CPU, and calls the dispatcher function to do the context switch. */
	
	virtual void resume(Thread * _thread);
	/* Add the given thread to the ready queue of the RRScheduler. This is called
      for threads that were waiting for an event to happen, or that have 
      to give up the CPU in response to a preemption. */
	
	virtual void add(Thread * _thread);
	/* Make the given thread runnable by the scheduler. This function is called
      after thread creation. */
	
	virtual void terminate(Thread * _thread);
	/* Remove the given thread from the scheduler in preparation for destruction
      of the thread. */
	
	virtual void handle_interrupt(REGS * _regs);
	/* The End of Quantum interrupt handler is called using this method. */
};

#endif

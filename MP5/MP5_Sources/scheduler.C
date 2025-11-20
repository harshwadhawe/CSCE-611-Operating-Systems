/*
 File: scheduler.C
 
 Author: Harsh Wadhawe
 Date  : 11/09/2025
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_timer.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler()
{
	qsize = 0;
	Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield()
{
	// Disable interrupts before modifying the ready queue to ensure atomicity
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	if (qsize == 0)
	{
		// Ready queue is empty — no runnable threads available
		// Console::puts("Queue is empty. No threads available.\n");
	}
	else
	{
		// Dequeue the next ready thread for execution
		Thread* new_thread = ready_queue.dequeue();
		
		// Update the ready queue size after removing a thread
		qsize -= 1;
		
		// Re-enable interrupts before resuming normal execution
		if (!Machine::interrupts_enabled())
		{
			Machine::enable_interrupts();
		}
		
		// Perform a context switch to the selected thread
		Thread::dispatch_to(new_thread);
	}
}


void Scheduler::resume(Thread* _thread)
{
	// Disable interrupts before modifying the ready queue to maintain consistency
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	// Place the specified thread back into the ready queue
	ready_queue.enqueue(_thread);
	
	// Update the ready queue size after adding a thread
	qsize += 1;
	
	// Re-enable interrupts after completing the queue operation
	if (!Machine::interrupts_enabled())
	{
		Machine::enable_interrupts();
	}
}


void Scheduler::add(Thread* _thread)
{
	// Disable interrupts before performing any operations on the ready queue
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	// Enqueue the new thread to make it runnable
	ready_queue.enqueue(_thread);
	
	// Update the ready queue size after insertion
	qsize += 1;
	
	// Re-enable interrupts after queue modification
	if (!Machine::interrupts_enabled())
	{
		Machine::enable_interrupts();
	}
}


void Scheduler::terminate(Thread* _thread)
{
	// Disable interrupts before modifying the ready queue to avoid race conditions
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	int index = 0;
	
	// Iterate through all threads in the ready queue
	// Remove the target thread and reinsert the others
	for (index = 0; index < qsize; index++)
	{
		Thread* top = ready_queue.dequeue();
		
		// If the thread does not match the one to terminate, reinsert it
		if (top->ThreadId() != _thread->ThreadId())
		{
			ready_queue.enqueue(top);
		}
		else
		{
			// Decrement the ready queue size for the removed thread
			qsize = qsize - 1;
		}
	}
	
	// Re-enable interrupts after all queue operations are complete
	if (!Machine::interrupts_enabled())
	{
		Machine::enable_interrupts();
	}
}


/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   RR S c h e d u l e r                                */
/*--------------------------------------------------------------------------*/

RRScheduler::RRScheduler()
{
	rr_qsize = 0;
	ticks = 0;
	hz = 5;		// Timer frequency (Hz) — 5 Hz corresponds to 50 ms per tick
	
	// Register this instance as the interrupt handler for interrupt code 0
	InterruptHandler::register_handler(0, this);
	
	// Configure the timer with the specified interrupt frequency
	set_frequency(hz);
}

void RRScheduler::set_frequency(int _hz)
{
	hz = _hz;
	int divisor = 1193180 / _hz;				// PIT input clock runs at ~1.19 MHz
	Machine::outportb(0x43, 0x34);				// Send command byte (channel 0, mode 2)
	Machine::outportb(0x40, divisor & 0xFF);	// Send low byte of divisor
	Machine::outportb(0x40, divisor >> 8);		// Send high byte of divisor
}

void RRScheduler::yield()
{
	// Send End-of-Interrupt (EOI) to the master interrupt controller
	Machine::outportb(0x20, 0x20);
	
	// Disable interrupts before modifying the ready queue
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	if (rr_qsize == 0)
	{
		// Ready queue is empty — no runnable threads available
		// Console::puts("Queue is empty. No threads available.\n");
	}
	else
	{
		// Dequeue the next ready thread for execution
		Thread* new_thread = ready_rr_queue.dequeue();
		
		// Reset tick counter for the next quantum
		ticks = 0;
		
		// Update ready queue size
		rr_qsize = rr_qsize - 1;
		
		// Re-enable interrupts before context switching
		if (!Machine::interrupts_enabled())
		{
			Machine::enable_interrupts();
		}
		
		// Perform a context switch to the selected thread
		Thread::dispatch_to(new_thread);
	}
}

void RRScheduler::resume(Thread* _thread)
{
	// Disable interrupts before modifying the ready queue
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	// Reinsert the specified thread into the ready queue
	ready_rr_queue.enqueue(_thread);
	
	// Update the ready queue size
	rr_qsize = rr_qsize + 1;
	
	// Re-enable interrupts after queue modification
	if (!Machine::interrupts_enabled())
	{
		Machine::enable_interrupts();
	}
}

void RRScheduler::add(Thread* _thread)
{
	// Disable interrupts before modifying the ready queue
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	// Add the new thread to the ready queue
	ready_rr_queue.enqueue(_thread);
	
	// Update the ready queue size
	rr_qsize = rr_qsize + 1;
	
	// Re-enable interrupts after modification
	if (!Machine::interrupts_enabled())
	{
		Machine::enable_interrupts();
	}
}

void RRScheduler::terminate(Thread* _thread)
{
	// Disable interrupts before modifying the ready queue
	if (Machine::interrupts_enabled())
	{
		Machine::disable_interrupts();
	}
	
	int index = 0;
	
	// Traverse the ready queue and remove the target thread
	for (index = 0; index < rr_qsize; index++)
	{
		Thread* top = ready_rr_queue.dequeue();
		
		// Retain threads that do not match the target
		if (top->ThreadId() != _thread->ThreadId())
		{
			ready_rr_queue.enqueue(top);
		}
		else
		{
			// Decrement ready queue size for the removed thread
			rr_qsize = rr_qsize - 1;
		}
	}
	
	// Re-enable interrupts after all operations are complete
	if (!Machine::interrupts_enabled())
	{
		Machine::enable_interrupts();
	}
}

void RRScheduler::handle_interrupt(REGS* _regs)
{
	// Increment tick count on each timer interrupt
	ticks += 1;
	
	// When the time quantum expires, preempt the running thread
	if (ticks >= hz)
	{
		// Reset tick counter
		ticks = 0;
		
		Console::puts("Time quantum (50 ms) has elapsed\n");
		
		// Move current thread back to ready queue and yield CPU
		resume(Thread::CurrentThread());
		yield();
	}
}



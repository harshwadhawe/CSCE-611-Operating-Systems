/*
 File: scheduler.C
 
 Author:
 Date  :
 
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

Scheduler::Scheduler() {
  /* Initialize the FIFO ready queue to empty state */
  ready_queue_head = nullptr;
  ready_queue_tail = nullptr;
  queue_size = 0;
  
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
  /* Called by the currently running thread to give up the CPU.
   * 
   * Algorithm:
   * 1. Get the current thread
   * 2. Add current thread to the ready queue (if it exists)
   * 3. Select the next thread from the ready queue
   * 4. If no thread is available, keep the current thread running
   * 5. Dispatch to the selected thread
   */
  
  Thread* current_thread = Thread::CurrentThread();
  
  /* If there's a current thread, add it back to the ready queue */
  if (current_thread != nullptr) {
    resume(current_thread);
  }
  
  /* Select the next thread from the ready queue */
  Thread* next_thread = nullptr;
  
  if (ready_queue_head != nullptr) {
    /* Dequeue the head of the queue (FIFO) */
    QueueNode* node_to_remove = ready_queue_head;
    next_thread = node_to_remove->thread;
    
    /* Update queue pointers */
    ready_queue_head = ready_queue_head->next;
    if (ready_queue_head == nullptr) {
      /* Queue is now empty */
      ready_queue_tail = nullptr;
    }
    
    /* Delete the node and update size */
    delete node_to_remove;
    queue_size--;
    
    /* Dispatch to the next thread */
    Thread::dispatch_to(next_thread);
  }
  /* If no thread in queue, the current thread continues running */
}

void Scheduler::resume(Thread * _thread) {
  /* Add the given thread to the ready queue.
   * This is called when:
   * - A thread that was waiting for an event becomes ready
   * - A thread voluntarily yields and needs to be added back
   * - A thread is preempted
   * 
   * We add threads to the tail of the queue to maintain FIFO order.
   */
  
  if (_thread == nullptr) {
    return;  /* Safety check: don't add null threads */
  }
  
  /* Create a new node for this thread */
  QueueNode* new_node = new QueueNode(_thread);
  
  if (ready_queue_tail == nullptr) {
    /* Queue is empty - this becomes both head and tail */
    ready_queue_head = new_node;
    ready_queue_tail = new_node;
  } else {
    /* Add to the tail of the queue (FIFO enqueue) */
    ready_queue_tail->next = new_node;
    ready_queue_tail = new_node;
  }
  
  queue_size++;
}

void Scheduler::add(Thread * _thread) {
  /* Make the given thread runnable by adding it to the ready queue.
   * This function is called after thread creation.
   * For FIFO scheduler, this is the same as resume().
   */
  
  resume(_thread);
}

void Scheduler::terminate(Thread * _thread) {
  
  if (_thread == nullptr) {
    return;  /* Safety check */
  }
  
  /* Search for the thread in the ready queue */
  QueueNode* current = ready_queue_head;
  QueueNode* previous = nullptr;
  
  while (current != nullptr) {
    if (current->thread == _thread) {
      /* Found the thread - remove it from the queue */
      
      if (previous == nullptr) {
        /* Thread is at the head of the queue */
        ready_queue_head = current->next;
        if (ready_queue_head == nullptr) {
          /* Queue is now empty */
          ready_queue_tail = nullptr;
        }
      } else {
        /* Thread is in the middle or tail */
        previous->next = current->next;
        if (current == ready_queue_tail) {
          /* Thread was at the tail */
          ready_queue_tail = previous;
        }
      }
      
      /* Delete the node and update size */
      delete current;
      queue_size--;
      return;  /* Thread found and removed */
    }
    
    previous = current;
    current = current->next;
  }
  
  /* Thread not found in queue - either it's currently running (self-termination)
   * or it's not in the scheduler. This is acceptable.
   */
}

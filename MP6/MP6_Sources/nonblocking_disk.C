/*
     File        : nonblocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "scheduler.H"
#include "system.H"
#include "interrupts.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
  : SimpleDisk(_size) {
  /* Initialize the blocked thread queue to empty state */
  blocked_queue_head = nullptr;
  blocked_queue_tail = nullptr;
  waiting_for_interrupt = false;
  
  /* Ensure scheduler is available (it should be created before the disk) */
  assert(System::SCHEDULER != nullptr);
  
  /* BONUS OPTION 3: Register as IRQ14 interrupt handler for disk interrupts
   * IRQ14 is the IDE primary channel interrupt, which fires when the disk
   * is ready for data transfer.
   */
  const unsigned int IRQ_DISK = 14;  /* IRQ14 is the disk interrupt */
  InterruptHandler::register_handler(IRQ_DISK, this);
  
  Console::puts("NonBlockingDisk: Registered as IRQ14 (disk) interrupt handler\n");
}

/*--------------------------------------------------------------------------*/
/* WAIT WHILE BUSY - NON-BLOCKING IMPLEMENTATION */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wait_while_busy() {
  /* BONUS OPTION 3: Interrupt-Driven Implementation
   * 
   * Override the base class's busy-waiting implementation to use
   * interrupt-driven I/O instead of polling.
   * 
   * Algorithm:
   * 1. Check if disk is busy
   * 2. If not busy, return immediately (disk is ready)
   * 3. If busy:
   *    a. Add current thread to blocked queue
   *    b. Set waiting_for_interrupt flag
   *    c. Yield the CPU (thread blocks, waiting for IRQ14 interrupt)
   *    d. When interrupt occurs, handle_interrupt() wakes this thread
   *    e. Thread resumes and checks disk status again
   *    f. If still busy, repeat; otherwise return
   * 
   * This eliminates both busy-waiting AND polling by using hardware
   * interrupts to notify when the disk is ready.
   */
  
  Thread* current_thread = Thread::CurrentThread();
  
  if (current_thread == nullptr || System::SCHEDULER == nullptr) {
    /* Fallback: if no scheduler or thread, use busy-wait (shouldn't happen) */
    while (is_busy()) {
      /* Minimal busy-wait as fallback */
    }
    return;
  }
  
  /* Loop until disk is ready */
  while (is_busy()) {
    /* Disk is busy - block thread and wait for interrupt */
    
    /* Check if thread is already in blocked queue (avoid duplicates) */
    bool thread_in_queue = false;
    BlockedThreadNode* node = blocked_queue_head;
    while (node != nullptr) {
      if (node->thread == current_thread) {
        thread_in_queue = true;
        break;
      }
      node = node->next;
    }
    
    /* Add current thread to blocked queue if not already there */
    if (!thread_in_queue) {
      BlockedThreadNode* new_node = new BlockedThreadNode(current_thread);
      
      if (blocked_queue_tail == nullptr) {
        /* Queue is empty - this becomes both head and tail */
        blocked_queue_head = new_node;
        blocked_queue_tail = new_node;
      } else {
        /* Add to the tail of the queue */
        blocked_queue_tail->next = new_node;
        blocked_queue_tail = new_node;
      }
      
      /* Set flag to indicate we're waiting for an interrupt */
      waiting_for_interrupt = true;
    }
    
    /* Yield the CPU - thread will be woken by handle_interrupt() when
     * IRQ14 fires (disk becomes ready). The scheduler will resume this
     * thread, and we'll check disk status again in the while loop.
     */
    System::SCHEDULER->yield();
    
    /* When we resume (after interrupt), remove thread from blocked queue */
    BlockedThreadNode* prev = nullptr;
    node = blocked_queue_head;
    while (node != nullptr) {
      if (node->thread == current_thread) {
        /* Found the thread - remove it */
        if (prev == nullptr) {
          /* Thread is at the head */
          blocked_queue_head = node->next;
          if (blocked_queue_head == nullptr) {
            blocked_queue_tail = nullptr;
            waiting_for_interrupt = false;  /* No more threads waiting */
          }
        } else {
          prev->next = node->next;
          if (node == blocked_queue_tail) {
            blocked_queue_tail = prev;
          }
        }
        delete node;
        break;
      }
      prev = node;
      node = node->next;
    }
    
    /* Check disk status again in the while loop condition */
    /* If disk became ready due to interrupt, we'll exit the loop */
  }
  
  /* Disk is ready - return to caller */
  waiting_for_interrupt = false;
}

/*--------------------------------------------------------------------------*/
/* WAKE NEXT BLOCKED THREAD */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wake_next_blocked_thread() {
  /* Wake up the next thread waiting for the disk.
   * This function is called by handle_interrupt() when the disk becomes ready.
   * It moves the waiting thread from the blocked queue back to the ready queue.
   */
  
  /* Check if there are blocked threads waiting */
  if (blocked_queue_head != nullptr && System::SCHEDULER != nullptr) {
    /* Remove the first blocked thread from the queue */
    BlockedThreadNode* node_to_wake = blocked_queue_head;
    blocked_queue_head = blocked_queue_head->next;
    
    if (blocked_queue_head == nullptr) {
      /* Queue is now empty */
      blocked_queue_tail = nullptr;
      waiting_for_interrupt = false;
    }
    
    /* Add the thread back to the scheduler's ready queue */
    /* The thread will resume execution and check disk status again */
    System::SCHEDULER->resume(node_to_wake->thread);
    
    /* Delete the node */
    delete node_to_wake;
  }
}

/*--------------------------------------------------------------------------*/
/* INTERRUPT HANDLER - BONUS OPTION 3 */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::handle_interrupt(REGS * _regs) {
  /* BONUS OPTION 3: Interrupt-Driven I/O Bottom Half
   * 
   * This function is called when IRQ14 (disk interrupt) fires, indicating
   * that the disk has completed its operation and is ready for data transfer.
   * 
   * Architecture:
   * - Top-half: read()/write() issue commands via ide_ata_issue_command()
   * - Bottom-half: This function wakes waiting threads when disk is ready
   * 
   * Algorithm:
   * 1. Check if disk is ready (not busy)
   * 2. If ready and there are blocked threads, wake the next one
   * 3. The woken thread will check disk status and continue with data transfer
   * 
   * Note: The actual data transfer (reading/writing bytes) happens in the
   * top-half (read()/write() functions) after the thread wakes up.
   */
  
  /* Check if disk is ready for data transfer */
  if (!is_busy()) {
    /* Disk is ready - wake up the next waiting thread */
    wake_next_blocked_thread();
  }
  
  /* Note: If disk is still busy, the interrupt might have been spurious
   * or the disk needs more time. The thread will check again when it
   * resumes (it's still in the blocked queue).
   */
}

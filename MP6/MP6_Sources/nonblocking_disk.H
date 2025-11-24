/*
     File        : nonblocking_disk.H

     Author      : 

     Date        : 
     Description : 

*/

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES FOR SCHEDULER AND INTERRUPTS */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "system.H"
#include "interrupts.H"

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

/* BONUS OPTION 3: Interrupt-Driven I/O Implementation
 * 
 * NonBlockingDisk now inherits from both SimpleDisk and InterruptHandler
 * to support interrupt-driven disk operations using IRQ14.
 * 
 * Architecture:
 * - Top-half: read()/write() issue commands and block threads
 * - Bottom-half: handle_interrupt() wakes threads when disk is ready
 * - Eliminates polling: threads are woken immediately when disk becomes ready
 */
class NonBlockingDisk : public SimpleDisk, public InterruptHandler {
private:
  /* Blocked Thread Queue
   * This queue holds threads that are waiting for the disk to become ready.
   * Since only one thread accesses the disk at a time (base implementation),
   * this queue will typically have at most one thread.
   */
  struct BlockedThreadNode {
    Thread* thread;                    /* Pointer to the blocked thread */
    BlockedThreadNode* next;           /* Pointer to the next node */
    
    BlockedThreadNode(Thread* t) : thread(t), next(nullptr) {}
  };
  
  BlockedThreadNode* blocked_queue_head;  /* Head of the blocked thread queue */
  BlockedThreadNode* blocked_queue_tail; /* Tail for efficient enqueue */
  
  /* Flag to track if we're waiting for an interrupt */
  bool waiting_for_interrupt;  /* True when a thread is blocked waiting for disk */
  
protected:
  /* Override wait_while_busy() to use interrupt-driven approach.
   * Threads block and wait for IRQ14 interrupt instead of polling.
   */
  virtual void wait_while_busy();
  
  /* Helper function to wake up the next waiting thread (if any) */
  void wake_next_blocked_thread();
  
public:
  /* Interrupt Handler Implementation (BONUS OPTION 3) */
  virtual void handle_interrupt(REGS * _regs);
  /* This is called when IRQ14 (disk interrupt) occurs.
   * When the disk becomes ready, this function:
   * 1. Checks if disk is ready
   * 2. Wakes up the waiting thread (if any)
   * 3. Allows the thread to continue with data transfer
   */
  
public:
   NonBlockingDisk(unsigned int _size); 
   /* Creates a NonBlockingDisk device with the given size connected to the 
      MASTER slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

};

#endif

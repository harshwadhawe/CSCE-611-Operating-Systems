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
/* INCLUDES FOR SCHEDULER */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "system.H"

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk : public SimpleDisk {
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
  
protected:
  /* Override wait_while_busy() to eliminate busy-waiting.
   * Instead of busy-looping, threads yield the CPU when the disk is busy
   * and check again when they resume.
   */
  virtual void wait_while_busy();
  
  /* Helper function to wake up the next waiting thread (if any) */
  void wake_next_blocked_thread();
  
public:
   NonBlockingDisk(unsigned int _size); 
   /* Creates a NonBlockingDisk device with the given size connected to the 
      MASTER slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

};

#endif

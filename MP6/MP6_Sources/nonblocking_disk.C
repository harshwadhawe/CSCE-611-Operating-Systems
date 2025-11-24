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

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
  : SimpleDisk(_size) {
  /* Initialize the blocked thread queue to empty state */
  blocked_queue_head = nullptr;
  blocked_queue_tail = nullptr;
  
  /* Ensure scheduler is available (it should be created before the disk) */
  assert(System::SCHEDULER != nullptr);
}

/*--------------------------------------------------------------------------*/
/* WAIT WHILE BUSY - NON-BLOCKING IMPLEMENTATION */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wait_while_busy() {
  /* Override the base class's busy-waiting implementation.
   * 
   * Algorithm:
   * 1. Check if disk is busy
   * 2. If not busy, return immediately (disk is ready)
   * 3. If busy:
   *    a. Check if current thread is already in blocked queue (to avoid duplicates)
   *    b. If not in queue, add current thread to blocked queue
   *    c. Yield the CPU (thread will be resumed later by scheduler)
   *    d. When thread resumes, remove it from blocked queue and check again
   *    e. Loop back to step 1 until disk is ready
   * 
   * This eliminates busy-waiting by allowing other threads to run
   * while waiting for the disk to become ready.
   * 
   * Note: The thread will keep checking when it resumes until the disk
   * is ready. This is a polling approach that trades CPU efficiency
   * (no busy-waiting) for slightly delayed response time.
   */
  
  Thread* current_thread = Thread::CurrentThread();
  
  if (current_thread == nullptr || System::SCHEDULER == nullptr) {
    /* Fallback: if no scheduler or thread, use busy-wait (shouldn't happen) */
    /* This should not occur in normal operation */
    while (is_busy()) {
      /* Minimal busy-wait as fallback */
    }
    return;
  }
  
  /* Check if thread is already in blocked queue (to avoid adding duplicates) */
  bool thread_in_queue = false;
  BlockedThreadNode* node = blocked_queue_head;
  while (node != nullptr) {
    if (node->thread == current_thread) {
      thread_in_queue = true;
      break;
    }
    node = node->next;
  }
  
  /* Loop until disk is ready */
  while (is_busy()) {
    /* Disk is busy - we need to wait */
    
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
      thread_in_queue = true;
    }
    
    /* Yield the CPU - scheduler will resume another thread */
    /* Note: The scheduler's yield() will add this thread to the ready queue,
     * which is fine - when it runs again, it will check disk status.
     */
    System::SCHEDULER->yield();
    
    /* When we resume, remove thread from blocked queue and check disk status again */
    /* Remove thread from blocked queue (if it's still there) */
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
          }
        } else {
          prev->next = node->next;
          if (node == blocked_queue_tail) {
            blocked_queue_tail = prev;
          }
        }
        delete node;
        thread_in_queue = false;  /* Reset flag since thread is no longer in queue */
        break;
      }
      prev = node;
      node = node->next;
    }
    
    /* Check disk status again in the while loop condition */
    /* If disk is still busy, we'll add thread back to queue in next iteration */
  }
  
  /* Disk is ready - return to caller */
}

/*--------------------------------------------------------------------------*/
/* WAKE NEXT BLOCKED THREAD */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::wake_next_blocked_thread() {
  /* Wake up the next thread waiting for the disk.
   * This function checks if the disk is ready and if there are any
   * blocked threads, it moves them back to the ready queue.
   * 
   * Note: In the base implementation, we use a polling approach where
   * threads check disk status when they resume. This function provides
   * a mechanism for more sophisticated implementations (e.g., interrupt-driven).
   */
  
  /* Check if disk is ready and there are blocked threads */
  if (!is_busy() && blocked_queue_head != nullptr && System::SCHEDULER != nullptr) {
    /* Remove the first blocked thread from the queue */
    BlockedThreadNode* node_to_wake = blocked_queue_head;
    blocked_queue_head = blocked_queue_head->next;
    
    if (blocked_queue_head == nullptr) {
      /* Queue is now empty */
      blocked_queue_tail = nullptr;
    }
    
    /* Add the thread back to the scheduler's ready queue */
    System::SCHEDULER->resume(node_to_wake->thread);
    
    /* Delete the node */
    delete node_to_wake;
  }
}

/*
 File: ContFramePool.C
 
 Author: Harsh Wadhawe
 Date  : 09/21/25
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
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
ContFramePool* ContFramePool::frame_pools[16];
int ContFramePool::num_pools = 0;


/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    info_frame_no = _info_frame_no;

    // Add the pool to a static framepool list
    assert(num_pools < 16);
    frame_pools[num_pools++] = this;

    // Set up a bitmap location

    if (_info_frame_no == 0) {
        // Check for first frame of pool for info.
        bitmap  = (unsigned char *) (base_frame_no * FRAME_SIZE);
        set_state(0, FrameState::HoS);
    } else {
        // external frame used for management info.
        bitmap = (unsigned char *)(_info_frame_no * FRAME_SIZE);
    }
    
    // Initialize all frames as free 
    for (unsigned long i = 0; i < nframes; i++) {
        if(_info_frame_no == 0 & i == 0) {
            // skip the info frame as its marked by HoS
            continue;
        }
        set_state(i, FrameState::Free);
    }


}

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    assert(_frame_no < nframes);

    return static_cast<FrameState>(bitmap[_frame_no]);
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{
    assert(_frame_no < nframes);

    bitmap[_frame_no] =  static_cast<unsigned char>(_state);
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // Looks for contiguous sequence of free frames
    for (unsigned long i = 0; i <= nframes - _n_frames; i++) {
        bool found_frame = true;
        
        // Check starting at i if we have _n_frames consecutive free frames 
        for (unsigned int j = 0; j < _n_frames; j++) {
            if (get_state(i + j) != FrameState::Free) {
                found_frame = false;
                break;
            }
        }
        
        if (found_frame) {
            // Mark the first frame as Head of Sequence (HoS)
            set_state(i, FrameState::HoS);
            
            // Mark the remaining frames as Used
            for (unsigned int j = 1; j < _n_frames; j++) {
                set_state(i + j, FrameState::Used);
            }
            
            // Returns  actual frame number
            return base_frame_no + i;
        }
    }
    
    //  No contiguous sequence is found
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Convert absolute frame value to relative frame value
    unsigned long relative_frame_no = _base_frame_no - base_frame_no;
    
    // Mark first frame as Head of Sequence (HoS)
    set_state(relative_frame_no, FrameState::HoS);
    
    // Mark remaining frames as Used
    for (unsigned long i = 1; i < _n_frames; i++) {

        set_state(relative_frame_no + i, FrameState::Used);

    }
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // Find which pool owns this frame
    ContFramePool* owning_pool = nullptr;

    for (int i = 0; i < num_pools; i++) {

        ContFramePool* pool = frame_pools[i];

        if (_first_frame_no >= (pool -> base_frame_no) && 
            _first_frame_no < (pool -> base_frame_no) + (pool -> nframes)) {

            owning_pool = pool;

            break;

        }
    }
    
    // Frame not found in any pool
    assert(owning_pool != nullptr);
    
    unsigned long relative_frame = _first_frame_no - (owning_pool -> base_frame_no);
    
    // Check if this is indeed a head of sequence
    assert(owning_pool -> get_state(relative_frame) == FrameState::HoS);
    
    // Mark the first frame as free
    owning_pool -> set_state(relative_frame, FrameState::Free);
    
    // Continue marking subsequent frames as free until we get a free frame or HoS
    unsigned long i = relative_frame + 1;

    while (i < (owning_pool -> nframes) && owning_pool -> get_state(i) == FrameState::Used) {

        owning_pool -> set_state(i, FrameState::Free);

        i++;
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    // Each frame is FRAME_SIZE bytes
    
    return (_n_frames + FRAME_SIZE - 1) / FRAME_SIZE;  // Round up
}

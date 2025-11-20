/*
 File: ContFramePool.C
 
 Author: Harsh Wadhawe
 Date  : 05/10/2025
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame-pool that allocates 
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
 need for the management of the frame-pool, if any.
 
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
 not associated with a particular frame-pool.
 
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

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

ContFramePool * ContFramePool::head = nullptr;

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    base_frame_num = _base_frame_no;

    nframes = _n_frames;

    info_frame_num = _info_frame_no;

    num_free_frames = _n_frames;
    
    // If _info_frame_no == zero: keep mgmt info in the first frame else : use the provided frame for mgmt info
    if (info_frame_num == 0) {
        bitmap = (unsigned char *) (base_frame_num * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_num * FRAME_SIZE);
    }
    
    assert((nframes % 8) == 0);
    
    // Initializing all bits in bitmap to zero
    for(int frame_num = 0; frame_num < _n_frames; frame_num++) {
        set_state(frame_num, FrameState::Free);
    }
    
    // Mark the first frame as being used if it is being used
    if(_info_frame_no == 0) {
        set_state(0, FrameState::Used);
        num_free_frames = num_free_frames - 1;
    }
    
    // Creating a ll and adding a new frame-pool
    if(head == nullptr) {
        head = this;
        head -> next = nullptr;
    } else {
        // Adding new frame-pool to existing ll
        ContFramePool * tmp = nullptr;
        for(tmp = head; tmp -> next != nullptr; tmp = tmp -> next);
        tmp -> next = this;
        tmp = this;
        tmp -> next = nullptr;
    }
    
    Console::puts("Frame-Pool initialized\n");
}


ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    unsigned int bitmap_row_idx = (_frame_no / 4);

    unsigned int bitmap_col_idx = ((_frame_no % 4) * 2);

    unsigned char mask_val = (bitmap[bitmap_row_idx] >> (bitmap_col_idx)) & 0b11;

    FrameState state_val = FrameState::Used;


    if(mask_val == 0b00) {
        state_val = FrameState::Free;
    }
    else if(mask_val == 0b01) {
        state_val = FrameState::Used;
    }
    else if(mask_val == 0b11) {
        state_val = FrameState::HoS;
    }

    return state_val;
}


void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{    
    unsigned int bitmap_row_idx = (_frame_no / 4);
    unsigned int bitmap_col_idx = ((_frame_no % 4) * 2);
    
    switch(_state) {
        case FrameState::Free:
            bitmap[bitmap_row_idx] &= ~(3 << bitmap_col_idx);
            break;
        case FrameState::Used:
            bitmap[bitmap_row_idx] ^= (1 << bitmap_col_idx);
            break;
        case FrameState::HoS:
            bitmap[bitmap_row_idx] ^= (3 << bitmap_col_idx);
            break;
    }


    return;
}


unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{    
    if((_n_frames > num_free_frames) || (_n_frames > nframes)) {
        Console::puts("ContFramePool::get_frames Invalid Request - Not enough free frames available!\n ");
        assert(false);
        return 0;
    }
    
    unsigned int idx = 0;
    unsigned int free_frames_start = 0;
    unsigned int available_flag = 0;
    unsigned int free_frames_count = 0;
    unsigned int output = 0;
    
    for(idx = 0; idx < nframes; idx++) {
        if(get_state(idx) == FrameState::Free)
        {
            if(free_frames_count == 0)
            {
                // Save free frames to start frame number.
                free_frames_start = idx;
            }
            
            free_frames_count = free_frames_count + 1;
            
            // If free_frames_count is equal to the required num of frames
            if(free_frames_count == _n_frames) {
                available_flag = 1;
                break;
            }
        }
        else {
            free_frames_count = 0;
        }
    }
    
    if(available_flag == 1) {
        // Contiguous frames are available from free_frames_start
        for(idx = free_frames_start; idx < (free_frames_start + _n_frames); idx++) {
            if(idx == free_frames_start) {
                set_state(idx, FrameState::HoS);
            } else {
                set_state(idx, FrameState::Used);
            }
        }
        
        num_free_frames = num_free_frames - _n_frames;
        output = free_frames_start + base_frame_num;
    }
    else {
        output = 0;
        Console::puts("ContframePool::get_frames - Contiguous free frames not available\n");
        assert(false);
    }
    
    return output;
}


void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{    
    if(    (_base_frame_no + _n_frames) > (base_frame_num + nframes) || (_base_frame_no < base_frame_num)) {

        Console::puts("ContframePool::mark_inaccessible - Cannot mark inacessible.\n");
        assert(false);
        return;
    }

    unsigned int idx = 0;

    for(idx = _base_frame_no; idx < (_base_frame_no + _n_frames); idx++) {

        if(get_state(idx - base_frame_num) == FrameState::Free)
        {
            if(idx == _base_frame_no) {
                set_state((idx - base_frame_num), FrameState::HoS);
            }
            else {
                set_state((idx - base_frame_num), FrameState::Used);
            }
            
            num_free_frames = num_free_frames - 1;
        }
    }
    
    return;
}


void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    unsigned int found_frame = 0;
    ContFramePool * tmp = head;

    // To find which pool the frame belongs to
    while(tmp != nullptr) {
        if((_first_frame_no >= tmp -> base_frame_num) && (_first_frame_no < (tmp -> base_frame_num + tmp -> nframes)))
        {
            found_frame = 1;
            tmp->release_frames_in_pool(_first_frame_no);
            break;
        }
        
        tmp = tmp -> next;
    }
    
    if(found_frame == 0) {
        Console::puts("ContframePool::release_frames - Cannot release frame since frame is not found in frame pools.\n");
        assert(false);
    }
    
    return;
}

void ContFramePool::release_frames_in_pool(unsigned long _first_frame_no)
{
    unsigned int idx = 0;
    
    // getState of frame
    if(get_state(_first_frame_no - base_frame_num) == FrameState::HoS) {
        for(idx = _first_frame_no; idx < (_first_frame_no + nframes); idx++) {
            set_state((idx - base_frame_num), FrameState::Free);
            num_free_frames = num_free_frames + 1;
        }
    }
    else {
        Console::puts("ContframePool::release_frames_in_pool - Cannot release frame since frame state is not HoS.\n");
        assert(false);
    }
}


unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{    
    // Use 2bits per frame
    return ((_n_frames * 2) / (4 * 1024 * 8)) + (((_n_frames * 2) % (4 * 1024 * 8)) > 0 ? 1 : 0);
}

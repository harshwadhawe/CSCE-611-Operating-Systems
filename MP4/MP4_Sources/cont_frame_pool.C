/*
 File: ContFramePool.C
 
 Author: Harsh Wadhawe
 Date  : 10/26/2025
 
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

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/
ContFramePool * ContFramePool::head = nullptr;

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // Initialize member variables
	base_frame_no = _base_frame_no;
	n_frames = _n_frames;
	info_frame_no = _info_frame_no;
	num_free_frames = _n_frames;
	
    // Determine where to store the management bitmap:
    // - If info_frame_no == 0 → use the first frame itself.
    // - Otherwise, use the provided info frame.
    
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    }
	else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }
	
    // Sanity check: total number of frames must be a multiple of 8
	assert((n_frames % 8) == 0);
	
    // If management info is stored in the first frame,
    // mark that frame as Used so it won't be allocated.
	for(int fno = 0; fno < _n_frames; fno++) {
        set_state(fno, FrameState::Free);
    }
	
	// We mark first frame as if it is being used
    if( _info_frame_no == 0 ) {
		set_state(0, FrameState::Used);
        num_free_frames -= 1;
    }
	
    // Insert this pool into the global linked list of pools
    if (head == nullptr) {
        head = this;
        head->next = nullptr;
    } else {
        // Find the tail and append this pool
        ContFramePool *tmp = nullptr;
        for (tmp = head; tmp->next != nullptr; tmp = tmp->next);
        tmp->next = this;
        tmp = this;
        tmp->next = nullptr;
    }
	
	Console::puts("Frame Pool initialized\n");
}


ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no)
{
    // Each frame uses 2 bits; 4 frames are packed in a single byte.
    unsigned int bitmap_row_index = _frame_no / 4;          // Index into bitmap array
    unsigned int bitmap_col_index = (_frame_no % 4) * 2;    // Bit offset (0, 2, 4, or 6)

    // Extract 2 bits representing the frame’s state
    unsigned char mask_result = (bitmap[bitmap_row_index] >> bitmap_col_index) & 0b11;

    // Default to Used (safe fallback)
    FrameState state_output = FrameState::Used;

#if DEBUG
    Console::puts("get_state row index = "); Console::puti(bitmap_row_index); Console::puts("\n");
    Console::puts("get_state col index = "); Console::puti(bitmap_col_index); Console::puts("\n");
    Console::puts("get_state bitmap value = "); Console::puti(bitmap[bitmap_row_index]); Console::puts("\n");
    Console::puts("get_state mask result = "); Console::puti(mask_result); Console::puts("\n");
#endif

    // Decode based on bit pattern
    if (mask_result == 0b00) {
        state_output = FrameState::Free;
#if DEBUG
        Console::puts("get_state state_output = Free\n");
#endif
    } else if (mask_result == 0b01) {
        state_output = FrameState::Used;
#if DEBUG
        Console::puts("get_state state_output = Used\n");
#endif
    } else if (mask_result == 0b11) {
        state_output = FrameState::HoS;
#if DEBUG
        Console::puts("get_state state_output = HoS\n");
#endif
    }

    return state_output;
}


void ContFramePool::set_state(unsigned long _frame_no, FrameState _state)
{
    // Each frame uses 2 bits; 4 frames fit in one byte of the bitmap.
    unsigned int bitmap_row_index = _frame_no / 4;          // Row index in bitmap array
    unsigned int bitmap_col_index = (_frame_no % 4) * 2;    // Bit offset within the row (0, 2, 4, or 6)

#if DEBUG
    Console::puts("set_state row index = "); Console::puti(bitmap_row_index); Console::puts("\n");
    Console::puts("set_state col index = "); Console::puti(bitmap_col_index); Console::puts("\n");
    Console::puts("set_state bitmap value before = "); Console::puti(bitmap[bitmap_row_index]); Console::puts("\n");
#endif

    switch (_state) {
        case FrameState::Free: {
            // Clear both bits (00) — frame marked Free
            bitmap[bitmap_row_index] &= ~(3 << bitmap_col_index);
            break;
        }
        case FrameState::Used: {
            // Set lower bit (01) — frame marked Used
            bitmap[bitmap_row_index] ^= (1 << bitmap_col_index);
            break;
        }
        case FrameState::HoS: {
            // Set both bits (11) — frame marked Head of Sequence
            bitmap[bitmap_row_index] ^= (3 << bitmap_col_index);
            break;
        }
    }

#if DEBUG
    Console::puts("set_state bitmap value after = "); Console::puti(bitmap[bitmap_row_index]); Console::puts("\n");
#endif
}


unsigned long ContFramePool::get_frames(unsigned int _n_frames) {
    // Quick sanity checks: do we even have enough total/free frames in this pool?
    if (_n_frames > num_free_frames || _n_frames > n_frames) {
        Console::puts("ContFramePool::get_frames Invalid Request - Not enough free frames available!\n");
        assert(false);
        return 0;
    }

    // Search state
    unsigned long run_start = 0;  // start index (relative to this pool) of the current free run
    unsigned long run_len   = 0;  // length of the current free run
    bool found              = false;

    // Scan for a contiguous run of _n_frames frames in state 'Free'
    for (unsigned long idx = 0; idx < n_frames; ++idx) {
        if (get_state(idx) == FrameState::Free) {
            if (run_len == 0) {
                // First free frame of a potential run
                run_start = idx;
            }
            ++run_len;

            if (run_len == _n_frames) {
                found = true;
                break; // run_start .. run_start + _n_frames - 1 is a match
            }
        } else {
            // Break in contiguity; reset and keep scanning
            run_len = 0;
        }
    }

    if (!found) {
        Console::puts("ContFramePool::get_frames - Continuous free frames not available\n");
        assert(false);
        return 0;
    }

    // Mark the allocated range:
    // - First frame becomes HoS (Head of Sequence)
    // - Remaining frames become Used
    const unsigned long run_end_exclusive = run_start + _n_frames;
    for (unsigned long idx = run_start; idx < run_end_exclusive; ++idx) {
        const FrameState target = (idx == run_start) ? FrameState::HoS : FrameState::Used;

#if DEBUG
        if (target == FrameState::HoS) {
            Console::puts("get_frames Operation = HoS\n");
        } else {
            Console::puts("get_frames Operation = Used\n");
        }
#endif
        set_state(idx, target);
    }

    // Update accounting and compute absolute (global) first frame number
    num_free_frames -= _n_frames;
    const unsigned long absolute_first = run_start + base_frame_no;

    return absolute_first;
}



void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Bounds of this pool: [pool_lower, pool_upper_exclusive)
    const unsigned long pool_lower = base_frame_no;
    const unsigned long pool_upper_exclusive = base_frame_no + n_frames;

    // Requested range: [_base_frame_no, end_index)
    const unsigned long end_index = _base_frame_no + _n_frames;

    // Validate that the requested range lies entirely within this pool
    if (end_index > pool_upper_exclusive || _base_frame_no < pool_lower)
    {
        Console::puts("ContFramePool::mark_inaccessible - Range out of bounds. "
                      "Cannot mark inaccessible.\n");
        assert(false);
        return;
    }

#if DEBUG
    Console::puts("Mark Inaccessible: _base_frame_no = ");
    Console::puti(_base_frame_no);
    Console::puts(" _n_frames = ");
    Console::puti(_n_frames);
    Console::puts("\n");
#endif

    // Walk frames in the requested range; mark the first as HoS, rest as Used.
    // We only transition Free -> HoS/Used and decrement num_free_frames upon change.
    for (unsigned long idx = _base_frame_no; idx < end_index; ++idx)
    {
        const unsigned long rel = idx - base_frame_no; // index relative to this pool

        if (get_state(rel) == FrameState::Free)
        {
            // First frame becomes Head-of-Sequence; subsequent frames become Used
            const FrameState target = (idx == _base_frame_no) ? FrameState::HoS
                                                              : FrameState::Used;

            set_state(rel, target);
            num_free_frames -= 1; // maintain free-frame accounting
        }
#if DEBUG
        else
        {
            Console::puts("ContFramePool::mark_inaccessible - Frame = ");
            Console::puti(idx);
            Console::puts(" already non-Free (likely already inaccessible).\n");
            assert(false);
        }
#endif
    }
}



void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    bool found_pool = false;          // Tracks if we locate the owning pool
    ContFramePool* current = head;    // Walk the intrusively linked list of pools

#if DEBUG
    Console::puts("In release_frames: First frame no =");
    Console::puti(_first_frame_no);
    Console::puts("\n");
#endif

    // Find which pool owns the given frame number
    while (current != nullptr)
    {
        const unsigned long lower = current->base_frame_no;                  // inclusive
        const unsigned long upper_exclusive = current->base_frame_no + current->n_frames; // exclusive

#if DEBUG
        Console::puts("In release_frames: Base frame lower =");
        Console::puti(lower);
        Console::puts("\n");
        Console::puts("In release_frames: Base frame upper =");
        Console::puti(upper_exclusive);
        Console::puts("\n");
#endif

        // Check membership: [lower, upper_exclusive)
        if (_first_frame_no >= lower && _first_frame_no < upper_exclusive)
        {
            found_pool = true;
            current->release_frames_in_pool(_first_frame_no);  // Delegate to the owning pool
            break;
        }

        current = current->next;  // Advance to next pool
    }

    // Fail fast if the frame doesn't belong to any pool
    if (!found_pool)
    {
        Console::puts("ContFramePool::release_frames - Cannot release frame. Frame not found in frame pools.\n");
        assert(false);
    }
}


void ContFramePool::release_frames_in_pool(unsigned long _first_frame_no)
{
    // Start checking from the frame immediately after the first frame
    unsigned long current_index = _first_frame_no + 1;

    // Check if the first frame is the Head of Sequence (HoS)
    if (get_state(_first_frame_no - base_frame_no) == FrameState::HoS)
    {
        // Mark the first frame as free
        set_state(_first_frame_no, FrameState::Free);
        num_free_frames += 1; // Update free frame count

        // Continue releasing subsequent frames until a free frame is encountered
        while (get_state(current_index - base_frame_no) != FrameState::Free)
        {
            // Mark current frame as free
            set_state(current_index, FrameState::Free);
            num_free_frames += 1; // Update free frame count

            // Move to the next frame
            current_index += 1;
        }
    }
    else
    {
        // Invalid release attempt — the frame to release is not a Head of Sequence
        Console::puts("ContFramePool::release_frames_in_pool - "
                      "Cannot release frame. Frame state is not HoS.\n");
        assert(false); // Fail fast to catch logical errors during debugging
    }
}



unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{	
    // Each frame requires 2 bits of metadata
    const unsigned int BITS_PER_FRAME = 2;

    // Each info frame can hold (4 KB * 8 bits per byte) bits of metadata
    const unsigned int BITS_PER_INFO_FRAME = 4 * 1024 * 8; // 32,768 bits

    // Total bits required to represent metadata for all frames
    unsigned long total_bits_needed = _n_frames * BITS_PER_FRAME;

    // Calculate how many full info frames are required
    unsigned long full_frames = total_bits_needed / BITS_PER_INFO_FRAME;

    // Add one more frame if there are leftover bits
    unsigned long extra_frame = (total_bits_needed % BITS_PER_INFO_FRAME) > 0 ? 1 : 0;

    // Return total info frames required
    return full_frames + extra_frame;
}

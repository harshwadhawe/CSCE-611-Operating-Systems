/*
 File: vm_pool.C
 
 Author:
 Date  : 2024/09/20
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
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
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {

    // Initialize member variables
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;
    ptr_next_vm_pool = nullptr;
    num_regions = 0; // No virtual regions yet

    // Register this virtual memory pool with the page table
    page_table->register_pool(this);

    // Initialize the first region entry at the base address
    allocated_region_info* region = (allocated_region_info*)base_address;
    region[0].base_address = base_address;
    region[0].length = PageTable::PAGE_SIZE;
    ptr_vm_region = region;

    // First page is reserved for metadata
    num_regions += 1;

    // Update available memory after reserving the first page
    available_memory = size - PageTable::PAGE_SIZE;

    // Log confirmation message
    Console::puts("Constructed VMPool object successfully.\n");
}

unsigned long VMPool::allocate(unsigned long _size)
{
    unsigned long pages_count = 0;

    // Check if enough virtual memory is available for allocation
    if (_size > available_memory) {
        Console::puts("Error: Not enough virtual memory space available for allocation.\n");
        assert(false);
    }

    // Calculate the number of pages required for this allocation
    pages_count = (_size / PageTable::PAGE_SIZE) + ((_size % PageTable::PAGE_SIZE) > 0 ? 1 : 0);

    // Set base address for the new region (immediately after the previous one)
    ptr_vm_region[num_regions].base_address =
        ptr_vm_region[num_regions - 1].base_address + ptr_vm_region[num_regions - 1].length;

    // Set region length aligned to full pages
    ptr_vm_region[num_regions].length = pages_count * PageTable::PAGE_SIZE;

    // Update available memory after allocation
    available_memory -= pages_count * PageTable::PAGE_SIZE;

    // Increment total number of allocated regions
    num_regions += 1;

    // Log confirmation message
    Console::puts("Allocated new VM region successfully.\n");

    // Return base address of the newly allocated region
    return ptr_vm_region[num_regions - 1].base_address;
}


void VMPool::release(unsigned long _start_address)
{
    int index = 0;
    int region_no = -1;
    unsigned long page_count = 0;

    // Find the region that matches the given start address
    for (index = 1; index < num_regions; index++) {
        if (ptr_vm_region[index].base_address == _start_address) {
            region_no = index;
            break;
        }
    }

    // If no region found, log and exit safely
    if (region_no == -1) {
        Console::puts("Error: Attempted to release an unknown or invalid region.\n");
        assert(false);
        return;
    }

    // Calculate the number of pages to free for the region
    page_count = ptr_vm_region[region_no].length / PageTable::PAGE_SIZE;

    // Free all pages belonging to this region
    while (page_count > 0) {
        page_table->free_page(_start_address);
        _start_address += PageTable::PAGE_SIZE;
        page_count -= 1;
    }

    // Remove the region entry from the region table
    for (index = region_no; index < num_regions - 1; index++) {
        ptr_vm_region[index] = ptr_vm_region[index + 1];
    }

    // Reclaim the released memory into available pool
    available_memory += ptr_vm_region[region_no].length;

    // Decrement region count
    num_regions -= 1;

    // Log successful release
    Console::puts("Released VM region and reclaimed memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address)
{
    // Check if the address lies within this virtual memory pool’s range
    Console::puts("Verifying if address belongs to the VM pool region...\n");

    // Address is outside the allocated region
    if ((_address < base_address) || (_address >= (base_address + size))) {
        Console::puts("Address is outside the allocated VM pool range.\n");
        return false;
    }

    // Address is valid and belongs to this pool
    Console::puts("Address is valid and within the allocated VM pool.\n");
    return true;
}


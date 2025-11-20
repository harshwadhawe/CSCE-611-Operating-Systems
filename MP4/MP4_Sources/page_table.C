#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;
VMPool * PageTable::vm_pool_head = nullptr;

void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
	PageTable::kernel_mem_pool = _kernel_mem_pool;
	PageTable::process_mem_pool = _process_mem_pool;
	PageTable::shared_size = _shared_size;
	Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
    unsigned int index = 0;
    unsigned long address = 0;

    // Paging starts as disabled
    paging_enabled = 0;

    // Calculate number of frames to map for the shared region: 4 MB / 4 KB = 1024 frames
    unsigned long num_shared_frames = (PageTable::shared_size / PAGE_SIZE);

    // Allocate and initialize the Page Directory
    page_directory = (unsigned long *)(kernel_mem_pool -> get_frames(1) * PAGE_SIZE);

    // Self-reference the last entry of the Page Directory for recursive mapping
    page_directory[num_shared_frames - 1] = ((unsigned long)page_directory | 0b11);

    // Allocate and initialize the first Page Table
    unsigned long *page_table = (unsigned long *)(process_mem_pool -> get_frames(1) * PAGE_SIZE);

    // Mark the first Page Directory Entry (PDE) as valid and point to the Page Table
    page_directory[0] = ((unsigned long)page_table | 0b11);

    // Mark remaining PDEs (except last one) as invalid but supervisor-level R/W
    for (index = 1; index < num_shared_frames - 1; index++) {
        // Supervisor (bit 1) and R/W (bit 2) set; Present bit not set (invalid entry)
        page_directory[index] = (page_directory[index] | 0b10);
    }

    // Map the first 4 MB of memory in the Page Table
    // Each entry (4 KB) is marked as present and writable.
    for (index = 0; index < num_shared_frames; index++) {
        // Supervisor level (bit 1), R/W (bit 2), Present (bit 0) → 0b11
        page_table[index] = (address | 0b11);
        address += PAGE_SIZE;
    }

    Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
    // Set this page table as the currently active one
    current_page_table = this;

    // Load the page directory address into the CR3 register
    write_cr3((unsigned long)(current_page_table -> page_directory));

    // Log confirmation message
    Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
    // Set the paging bit (bit 31) in CR0 register
    write_cr0(read_cr0() | 0x80000000);

    // Update paging status flag
    paging_enabled = 1;

    // Log confirmation message
    Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
    unsigned long error_code = _r -> err_code;

    // Check if the fault is due to a page not present
    if ((error_code & 1) == 0) {
        // Read the faulting virtual address from CR2
        unsigned long fault_address = read_cr2();

        // Get the current page directory base from CR3
        unsigned long* page_dir = (unsigned long*)read_cr3();

        // Extract page directory index (top 10 bits)
        unsigned long page_dir_index = (fault_address >> 22);

        // Extract page table index (next 10 bits)
        unsigned long page_table_index = ((fault_address & (0x03FF << 12)) >> 12);

        unsigned long* new_page_table = nullptr;
        unsigned long* new_pde = nullptr;

        // Verify the faulting address belongs to a valid VM region
        unsigned int present_flag = 0;
        VMPool* tmp = PageTable::vm_pool_head;

        for (; tmp != nullptr; tmp = tmp -> ptr_next_vm_pool) {
            if (tmp -> is_legitimate(fault_address) == true) {
                present_flag = 1;
                break;
            }
        }

        // If no valid VM region found, abort
        if ((tmp != nullptr) && (present_flag == 0)) {
            Console::puts("Not a legitimate address.\n");
            assert(false);
        }

        // Check if page fault occurred due to an invalid PDE
        if ((page_dir[page_dir_index] & 1) == 0) {
            // Page directory entry (PDE) not present
            int index = 0;

            // Allocate a new page table
            new_page_table = (unsigned long*)(process_mem_pool -> get_frames(1) * PAGE_SIZE);

            // Access PDE via recursive mapping
            unsigned long* new_pde = (unsigned long*)(0xFFFFF << 12);
            new_pde[page_dir_index] = ((unsigned long)(new_page_table) | 0b11);

            // Initialize all PTEs in the new page table as invalid (user-level only)
            for (index = 0; index < 1024; index++) {
                new_page_table[index] = 0b100;
            }

            // Allocate a new physical frame to map this PTE
            new_pde = (unsigned long*)(process_mem_pool -> get_frames(1) * PAGE_SIZE);

            // Get PTE address via recursive mapping (1023 | PDE | offset)
            unsigned long* page_entry = (unsigned long*)((0x3FF << 22) | (page_dir_index << 12));

            // Mark the PTE as valid
            page_entry[page_table_index] = ((unsigned long)(new_pde) | 0b11);
        } else {
            // PDE is present, but PTE is invalid
            new_pde = (unsigned long*)(process_mem_pool -> get_frames(1) * PAGE_SIZE);

            // Get PTE address via recursive mapping (1023 | PDE | offset)
            unsigned long* page_entry = (unsigned long*)((0x3FF << 22) | (page_dir_index << 12));

            // Mark the PTE as valid
            page_entry[page_table_index] = ((unsigned long)(new_pde) | 0b11);
        }
    }

    Console::puts("Handled page fault\n");
}

void PageTable::register_pool(VMPool * _vm_pool)
{	
    // Register the first virtual memory pool
    if (PageTable::vm_pool_head == nullptr) {
        PageTable::vm_pool_head = _vm_pool;
    } 
    // Register additional pools by appending to the linked list
    else {
        VMPool* tmp = PageTable::vm_pool_head;
        for (; tmp -> ptr_next_vm_pool != nullptr; tmp = tmp -> ptr_next_vm_pool);
        tmp -> ptr_next_vm_pool = _vm_pool;
    }

    // Log confirmation message
    Console::puts("Registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no)
{
    // Extract page directory index (top 10 bits)
    unsigned long page_dir_index = (_page_no & 0xFFC00000) >> 22;

    // Extract page table index (next 10 bits)
    unsigned long page_table_index = (_page_no & 0x003FF000) >> 12;

    // Get the address of the page table entry (PTE) via recursive mapping
    unsigned long* page_table = (unsigned long*)((0x000003FF << 22) | (page_dir_index << 12));

    // Extract the physical frame number from the PTE
    unsigned long frame_no = ((page_table[page_table_index] & 0xFFFFF000) / PAGE_SIZE);

    // Release the frame back to the process memory pool
    process_mem_pool -> release_frames(frame_no);

    // Mark the PTE as invalid (set only the R/W bit, clear Present bit)
    page_table[page_table_index] = page_table[page_table_index] | 0b10;

    // Flush the TLB by reloading the current page table
    load();

    // Log confirmation message
    Console::puts("Freed page\n");
}

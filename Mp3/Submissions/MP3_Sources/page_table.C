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
   unsigned int idx = 0;

   unsigned long address = 0;
   
   // Initially paging is diabled
   paging_enabled = 0;
   
   // 4MB / 4KB = 1024bytes
   unsigned long num_shared_frames = (PageTable::shared_size / PAGE_SIZE);
   
   // Page dir
   page_directory = (unsigned long *)(kernel_mem_pool -> get_frames(1) * PAGE_SIZE);
   
   // Page table
   unsigned long * page_table = (unsigned long *)(kernel_mem_pool -> get_frames(1) * PAGE_SIZE);
   
   // PDE, 1st marked as valid
   page_directory[0] = ((unsigned long) page_table | 0b11);
   
   // rest of PDE's are marked as invalid
   for(idx = 1; idx < num_shared_frames; idx++) {
       page_directory[idx] = (page_directory[idx] | 0b10);    
   }
   
   // For 1st 4MB all pages are marked as valid
   for(idx = 0; idx < num_shared_frames; idx++) {
       page_table[idx] = (address | 0b11);                          
       address = address + PAGE_SIZE;
   }
   
   Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
   current_page_table = this;
   
   // Add PD address into CR3 register
   write_cr3((unsigned long)(current_page_table->page_directory));
   
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
    // Set the paging bit to 32nd bit
   write_cr0(read_cr0() | 0x80000000);    
   paging_enabled = 1;    // varaible for paging enabled
   
   Console::puts("Enabled paging\n");
}

// Helper: Invalidate a single TLB entry for the given virtual address.
static inline void invalidate_tlb_entry(void* virtual_address) {
#if defined(__i386__) || defined(__x86_64__)
    asm volatile("invlpg (%0)" :: "r"(virtual_address) : "memory");
#else
    // Fallback: Reload CR3 if the CPU lacks INVLPG instruction.
    write_cr3(read_cr3());
#endif
}

void PageTable::handle_fault(REGS* cpu_registers)
{
    // Extract fault details
    unsigned long error_code = cpu_registers->err_code;
    unsigned long fault_address = read_cr2();       // Address that triggered the fault
    unsigned long* page_directory = (unsigned long*)(read_cr3() & ~0xFFFUL);

    // Decode the error code bits (Intel manual §4.7)
    bool page_not_present = (error_code & 0x1) == 0;
    bool is_write_access  = (error_code & 0x2) != 0;
    bool from_user_mode   = (error_code & 0x4) != 0;

    // Bit masks for page table flags
    const unsigned long PRESENT = 0x001;
    const unsigned long WRITE   = 0x002;
    const unsigned long USER    = 0x004;

    const unsigned long KERNEL_RW = PRESENT | WRITE;         // Supervisor RW
    const unsigned long USER_RW   = PRESENT | WRITE | USER;  // User RW

    // Compute directory and table indices
    unsigned long page_directory_index = (fault_address >> 22) & 0x3FF;
    unsigned long page_table_index     = (fault_address >> 12) & 0x3FF;

    // We only handle "page not present" faults here.
    if (!page_not_present) {
        Console::puts("Protection fault (present page) — likely permission issue.\n");
        assert(false);
        return;
    }

    // If the page directory entry is not present, create a new page table.
    if ((page_directory[page_directory_index] & PRESENT) == 0) {
        unsigned long new_page_table_frame = kernel_mem_pool->get_frames(1);
        unsigned long new_page_table_phys  = new_page_table_frame * PAGE_SIZE;

        // Safety check: ensure identity-mapped region can access this physical frame.
        assert(new_page_table_phys + PAGE_SIZE <= PageTable::shared_size);

        // Install the PDE with appropriate flags (user if fault from user mode)
        page_directory[page_directory_index] =
            new_page_table_phys | (from_user_mode ? USER_RW : KERNEL_RW);

        // Zero-initialize the new page table entries (mark all as non-present)
        unsigned long* new_page_table = (unsigned long*)new_page_table_phys;
        for (unsigned int entry = 0; entry < 1024; ++entry)
            new_page_table[entry] = 0;
    }

    // Now ensure the specific page table entry is mapped.
    unsigned long* page_table =
        (unsigned long*)(page_directory[page_directory_index] & ~0xFFFUL);

    if ((page_table[page_table_index] & PRESENT) == 0) {
        unsigned long new_page_frame = process_mem_pool->get_frames(1);
        unsigned long new_page_phys  = new_page_frame * PAGE_SIZE;

        // Set proper permissions
        unsigned long pte_flags = from_user_mode ? USER_RW : KERNEL_RW;
        page_table[page_table_index] = new_page_phys | pte_flags;
    }

    // Flush the stale TLB entry so the CPU sees the new mapping.
    invalidate_tlb_entry((void*)fault_address);

    Console::puts("Handled page fault\n");
}

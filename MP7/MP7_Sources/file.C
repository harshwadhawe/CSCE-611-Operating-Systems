/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");
    fs = _fs;
    inode = fs -> LookupFile(_id);
    current_position = 0;
    cached_block_idx = (unsigned int)-1; // No block cached yet
    
    // Initialize cache to zeros (will be loaded on demand during read/write)
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) {
        block_cache[i] = 0;
    }
}

File::~File() {
    Console::puts("Closing file.\n");
    if (inode != nullptr && inode -> block_numbers_block != 0 && cached_block_idx != (unsigned int)-1) {
        // Write current cache block if we have a valid cached block
        unsigned int* block_nums = new unsigned int[SimpleDisk::BLOCK_SIZE / sizeof(unsigned int)];
        fs -> disk -> read(inode -> block_numbers_block, (unsigned char*)block_nums);
        if (cached_block_idx < inode -> num_blocks && block_nums[cached_block_idx] != 0) {
            fs -> disk -> write(block_nums[cached_block_idx], block_cache);
        }
        delete[] block_nums;
        fs -> SaveInodes();
    }
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");
    if (inode == nullptr || inode -> block_numbers_block == 0) {
        return 0;
    }
    
    unsigned int available = inode -> file_length - current_position;
    unsigned int to_read = (_n < available) ? _n : available;
    unsigned int bytes_read = 0;
    
    // Load block numbers
    unsigned int* block_nums = new unsigned int[SimpleDisk::BLOCK_SIZE / sizeof(unsigned int)];
    fs -> disk -> read(inode -> block_numbers_block, (unsigned char*)block_nums);
    
    while (bytes_read < to_read) {
        unsigned int block_idx = (current_position + bytes_read) / SimpleDisk::BLOCK_SIZE;
        unsigned int offset_in_block = (current_position + bytes_read) % SimpleDisk::BLOCK_SIZE;
        
        if (block_idx >= inode -> num_blocks || block_nums[block_idx] == 0) {
            break; // No more data
        }
        
        // Load the block if not already cached
        if (block_idx != cached_block_idx) {
            if (block_nums[block_idx] != 0) {
                fs -> disk -> read(block_nums[block_idx], block_cache);
            } else {
                for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) {
                    block_cache[i] = 0;
                }
            }
            cached_block_idx = block_idx;
        }
        
        // Copy from cache
        unsigned int remaining_in_block = SimpleDisk::BLOCK_SIZE - offset_in_block;
        unsigned int remaining_to_read = to_read - bytes_read;
        unsigned int copy_count = (remaining_in_block < remaining_to_read) ? remaining_in_block : remaining_to_read;
        
        for (unsigned int i = 0; i < copy_count; i++) {
            _buf[bytes_read + i] = block_cache[offset_in_block + i];
        }
        
        bytes_read += copy_count;
    }
    
    current_position += bytes_read;
    delete[] block_nums;
    return bytes_read;
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");
    if (inode == nullptr || inode -> block_numbers_block == 0) {
        return 0;
    }
    
    unsigned int max_file_size = Inode::MAX_BLOCKS * SimpleDisk::BLOCK_SIZE;
    unsigned int max_write = max_file_size - current_position;
    unsigned int to_write = (_n < max_write) ? _n : max_write;
    unsigned int bytes_written = 0;
    
    // Load block numbers
    unsigned int* block_nums = new unsigned int[SimpleDisk::BLOCK_SIZE / sizeof(unsigned int)];
    fs -> disk -> read(inode -> block_numbers_block, (unsigned char*)block_nums);
    
    while (bytes_written < to_write) {
        unsigned int block_idx = (current_position + bytes_written) / SimpleDisk::BLOCK_SIZE;
        unsigned int offset_in_block = (current_position + bytes_written) % SimpleDisk::BLOCK_SIZE;
        
        // Allocate new block if needed
        if (block_idx >= inode -> num_blocks) {
            if (inode -> num_blocks >= Inode::MAX_BLOCKS) {
                break; // Maximum file size reached
            }
            
            int new_block = fs -> GetFreeBlock();
            if (new_block == -1) {
                break; // No free blocks available
            }
            
            block_nums[inode -> num_blocks] = new_block;
            fs -> free_blocks[new_block] = 1;
            inode -> num_blocks++;
            
            // Initialize new block to zeros
            for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) {
                block_cache[i] = 0;
            }
            fs -> disk -> write(new_block, block_cache);
            
            // Save updated block numbers
            fs -> disk -> write(inode -> block_numbers_block, (unsigned char*)block_nums);
            fs -> SaveFreeList();
        }
        
        // Load the block if not already cached
        if (block_idx != cached_block_idx) {
            if (block_nums[block_idx] != 0) {
                fs -> disk -> read(block_nums[block_idx], block_cache);
            } else {
                for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE; i++) {
                    block_cache[i] = 0;
                }
            }
            cached_block_idx = block_idx;
        }
        
        // Copy to cache
        unsigned int remaining_in_block = SimpleDisk::BLOCK_SIZE - offset_in_block;
        unsigned int remaining_to_write = to_write - bytes_written;
        unsigned int copy_count = (remaining_in_block < remaining_to_write) ? remaining_in_block : remaining_to_write;
        
        for (unsigned int i = 0; i < copy_count; i++) {
            block_cache[offset_in_block + i] = _buf[bytes_written + i];
        }
        
        // Write block back to disk
        fs -> disk -> write(block_nums[block_idx], block_cache);
        
        bytes_written += copy_count;
    }
    
    current_position += bytes_written;
    if (current_position > inode -> file_length) {
        inode -> file_length = current_position;
    }
    
    delete[] block_nums;
    return bytes_written;
}

void File::Reset() {
    Console::puts("resetting file\n");
    current_position = 0;
    cached_block_idx = (unsigned int)-1; // Invalidate cache
}

bool File::EoF() {
    Console::puts("checking for EoF\n");
    if (inode == nullptr) {
        return true;
    }
    return current_position >= inode -> file_length;
}

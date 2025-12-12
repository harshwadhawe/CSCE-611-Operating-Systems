/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
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
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    disk = nullptr;
    size = 0;
    inodes = nullptr;
    free_blocks = nullptr;
}

FileSystem::~FileSystem() {
    if (disk != nullptr) {
        SaveInodes();
        SaveFreeList();
    }
    if (inodes != nullptr) {
        delete[] inodes;
    }
    if (free_blocks != nullptr) {
        delete[] free_blocks;
    }
}


/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/


bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system from disk\n");
    disk = _disk;
    size = disk -> NaiveSize();
    unsigned int num_blocks = size / SimpleDisk::BLOCK_SIZE;
    
    inodes = new Inode[MAX_INODES];
    free_blocks = new unsigned char[num_blocks];
    
    disk -> read(0, (unsigned char*)inodes);
    disk -> read(1, free_blocks);
    
    // Set fs pointer for all inodes
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        inodes[i].fs = this;
    }
    
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) {
    Console::puts("formatting disk\n");
    unsigned int num_blocks = _size / SimpleDisk::BLOCK_SIZE;
    Inode* temp_inodes = new Inode[MAX_INODES];
    unsigned char* temp_free_blocks = new unsigned char[num_blocks];
    
    // Initialize all inodes
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        temp_inodes[i].id = 0;
        temp_inodes[i].block_numbers_block = 0;
        temp_inodes[i].num_blocks = 0;
        temp_inodes[i].file_length = 0;
        temp_inodes[i].fs = nullptr;
    }
    
    // Initialize free blocks: 0 and 1 are used, rest are free
    for (unsigned int i = 0; i < num_blocks; i++) {
        temp_free_blocks[i] = (i < 2) ? 1 : 0;
    }
    
    // Write to disk
    _disk -> write(0, (unsigned char*)temp_inodes);
    _disk -> write(1, temp_free_blocks);
    
    delete[] temp_inodes;
    delete[] temp_free_blocks;
    return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file with id = "); Console::puti(_file_id); Console::puts("\n");
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == _file_id) {
            return &inodes[i];
        }
    }
    return nullptr;
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file with id:"); Console::puti(_file_id); Console::puts("\n");
    if (LookupFile(_file_id) != nullptr) {
        return false;
    }
    
    short inode_idx = GetFreeInode();
    if (inode_idx == -1) {
        return false;
    }
    
    // Allocate a block to store data block numbers
    int block_nums_block = GetFreeBlock();
    if (block_nums_block == -1) {
        return false;
    }
    
    // Initialize the block numbers block to zeros
    unsigned int* block_nums = new unsigned int[SimpleDisk::BLOCK_SIZE / sizeof(unsigned int)];
    for (unsigned int i = 0; i < SimpleDisk::BLOCK_SIZE / sizeof(unsigned int); i++) {
        block_nums[i] = 0;
    }
    disk -> write(block_nums_block, (unsigned char*)block_nums);
    delete[] block_nums;
    
    inodes[inode_idx].id = _file_id;
    inodes[inode_idx].block_numbers_block = block_nums_block;
    inodes[inode_idx].num_blocks = 0;
    inodes[inode_idx].file_length = 0;
    inodes[inode_idx].fs = this;
    
    free_blocks[block_nums_block] = 1;
    
    SaveInodes();
    SaveFreeList();
    return true;
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file with id:"); Console::puti(_file_id); Console::puts("\n");
    Inode* inode = LookupFile(_file_id);
    if (inode == nullptr) {
        return false;
    }
    
    // Free all data blocks
    if (inode -> block_numbers_block != 0) {
        unsigned int* block_nums = new unsigned int[SimpleDisk::BLOCK_SIZE / sizeof(unsigned int)];
        disk -> read(inode -> block_numbers_block, (unsigned char*)block_nums);
        
        for (unsigned int i = 0; i < inode -> num_blocks; i++) {
            if (block_nums[i] != 0) {
                free_blocks[block_nums[i]] = 0;
            }
        }
        
        // Free the block numbers block
        free_blocks[inode -> block_numbers_block] = 0;
        delete[] block_nums;
    }
    
    inode -> id = 0;
    inode -> block_numbers_block = 0;
    inode -> num_blocks = 0;
    inode -> file_length = 0;
    SaveInodes();
    SaveFreeList();
    return true;
}

short FileSystem::GetFreeInode() {
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == 0) {
            return i;
        }
    }
    return -1;
}

int FileSystem::GetFreeBlock() {
    unsigned int num_blocks = size / SimpleDisk::BLOCK_SIZE;
    for (unsigned int i = 2; i < num_blocks; i++) {
        if (free_blocks[i] == 0) {
            return i;
        }
    }
    return -1;
}

int FileSystem::GetFreeBlocks(unsigned int count, unsigned int* blocks) {
    unsigned int num_blocks = size / SimpleDisk::BLOCK_SIZE;
    unsigned int found = 0;
    for (unsigned int i = 2; i < num_blocks && found < count; i++) {
        if (free_blocks[i] == 0) {
            blocks[found++] = i;
        }
    }
    return (found == count) ? found : -1;
}

void FileSystem::SaveInodes() {
    if (disk != nullptr && inodes != nullptr) {
        disk -> write(0, (unsigned char*)inodes);
    }
}

void FileSystem::SaveFreeList() {
    if (disk != nullptr && free_blocks != nullptr) {
        disk -> write(1, free_blocks);
    }
}

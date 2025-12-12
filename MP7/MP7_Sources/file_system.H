/*
	File: file_system.H

	Author: R. Bettati
			Department of Computer Science
			Texas A&M University
	Date  : 24/11/22

	Description: Simple File System.


*/

#ifndef _FILE_SYSTEM_H_ // include file only once
#define _FILE_SYSTEM_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

class FileSystem;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

class Inode
{
	friend class FileSystem; // The inode is in an uncomfortable position between
	friend class File;       // File System and File. We give both full access
	// to the Inode.

private:
	long id; // File "name"
	static constexpr unsigned int MAX_BLOCKS = 128; // Maximum blocks per file (64kB = 128 * 512)
	unsigned int block_numbers_block; // Block number storing the array of data block numbers (0 = invalid)
	unsigned int num_blocks; // Number of blocks currently allocated
	unsigned int file_length; // Current file size in bytes (0-65536)

	FileSystem* fs; // Pointer to the File system

/* You may need a few additional functions to help read and store the
   inodes from and to disk. */
};

/*--------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS */
/*--------------------------------------------------------------------------*/

/* (none) */

/*--------------------------------------------------------------------------*/
/* F i l e S y s t e m  */
/*--------------------------------------------------------------------------*/

class FileSystem
{

	friend class Inode;
	friend class File;

private:
	/* -- DEFINE YOUR FILE SYSTEM DATA STRUCTURES HERE. */

	SimpleDisk* disk;
	unsigned int size;

	static constexpr unsigned int MAX_INODES = SimpleDisk::BLOCK_SIZE / sizeof(Inode);
	/* Inode uses indirect blocks, so it stays small enough to fit multiple inodes per block */

	Inode* inodes; 
	/* The inode list */

	unsigned char* free_blocks;
	/* The free-block list. You may want to implement the "list" as a bitmap.
	   Feel free to use an unsigned char to represent whether a block is free or not;
	   no need to go to bits if you don't want to.
	   If you reserve one block to store the "free list", you can handle a file system up to
	   256kB. (Large enough for a proof of concept.) */

	short GetFreeInode();
	int GetFreeBlock();
	int GetFreeBlocks(unsigned int count, unsigned int* blocks);
	void SaveFreeList();

public:
	void SaveInodes(); // Public so File can call it when closing
	FileSystem();
	/* Just initializes local data structures. Does not connect to disk yet. */

	~FileSystem();
	/* Unmount file system if it has been mounted. */

	bool Mount(SimpleDisk* _disk);
	/* Associates this file system with a disk. Limit to at most one file system per disk.
	   Returns true if operation successful (i.e. there is indeed a file system on the disk.) */

	static bool Format(SimpleDisk* _disk, unsigned int _size);
	/* Wipes any file system from the disk and installs an empty file system of given size. */

	Inode* LookupFile(int _file_id);
	/* Find file with given id in file system. If found, return its inode.
		 Otherwise, return null. */

	bool CreateFile(int _file_id);
	/* Create file with given id in the file system. If file exists already,
	   abort and return false. Otherwise, return true. */

	bool DeleteFile(int _file_id);
	/* Delete file with given id in the file system; free any disk block occupied by the file. */
};
#endif

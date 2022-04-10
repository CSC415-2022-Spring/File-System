/**************************************************************
* Class:  CSC-415-02 Fall 2021
* Names: Patrick Celedio, Chase Alexander, Gurinder Singh, Jonathan Luu
* Student IDs: 920457223, 921040156, 921369355, 918548844
* GitHub Name: csc415-filesystem-CalDevC
* Group Name: Sudoers
* Project: Basic File System
*
* File: fsInit.c
*
* Description: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "directory.h"
#include "fsLow.h"
#include "mfs.h"

// Magic number for fsInit.c
#define SIG 90982
#define FREE_SPACE_START_BLOCK 1

// This will help us determine the int block in which we
// found a bit of value 1 representing free block
int intBlock = 0;

// Pointer to our root directory (hash table of directory entries)
hashTable* rootDir;

int blockSize;

struct volumeCtrlBlock {
  long signature;      //Marker left behind that can be checked
                       //to know if the disk is setup correctly 
  int blockSize;       //The size of each block in bytes
  long blockCount;	   //The number of blocks in the file system
  long numFreeBlocks;  //The number of blocks not in use
  int rootDir;		     //Block number where root starts
  int freeBlockNum;    //To store the block number where our bitmap starts
} volumeCtrlBlock;

int getFreeBlockNum(int numOfInts, int* bitVector) {
  //**********Get the free block number ***********
  // This will help determine the first block number that is
  // free
  int freeBlock = 0;

  //****Calculate free space block number*****
  // We can use the following formula to calculate the block
  // number => (32 * i) + (32 - j), where (32 * i) will give us 
  // the number of 32 bit blocks where we found a bit of value 1
  // and we add (31 - j) which is a offset to get the block number 
  // it represents within that 32 bit block
  for (int i = 0; i < numOfInts; i++) {
    for (int j = 31; j >= 0; j--) {
      if (bitVector[i] & (1 << j)) {
        intBlock = i;
        freeBlock = (intBlock * 32) + (31 - j);
        return freeBlock;
      }
    }
  }
}

void setBlocksAsAllocated(int freeBlock, int blocksAllocated, int* bitVector) {
  // Set the number of bits specified in the blocksAllocated
  // to 0 starting from freeBlock
  freeBlock += 1;

  for (int i = freeBlock; i < (freeBlock + blocksAllocated); i++) {
    bitVector[intBlock] = bitVector[intBlock] & ~(1 << (32 - i));
  }
}

//Write all directory entries in the hashTable to the disk
void writeTableData(hashTable* table, int lbaCount, int lbaPosition, int blockSize) {
  dirEntry* arr = malloc(lbaCount * blockSize);

  int j = 0;  //j will track indcies for the array

  //iterate through the whole table to find every directory entry that is in use
  for (int i = 0; i < SIZE; i++) {
    node* entry = table->entries[i];
    if (strcmp(entry->value->filename, "") != 0) {
      arr[j] = *entry->value;
      j++;

      //add other entries that are at the same hash location
      while (entry->next != NULL) {
        entry = entry->next;
        arr[j] = *entry->value;
        j++;
      }
    }

    // Don't bother lookng through rest of table if all entries are found
    if (j == table->numEntries) {
      break;
    }
  }

  //Write to the array out to the specified block numbers
  printf("Write root directory to: %d, for number of blocks: %d\n",
    lbaPosition, lbaCount);
  LBAwrite(arr, lbaCount, lbaPosition);
}

//Read all directory entries from a certain disk location into a new hash table
hashTable* readTableData(int lbaCount, int lbaPosition, int blockSize) {
  //Read all of the entries from disk into an array
  dirEntry* arr = malloc(lbaCount * blockSize);
  LBAread(arr, lbaCount, lbaPosition);

  //Create a new hash table to be populated
  hashTable* dirPtr = hashTableInit((lbaCount * blockSize) / sizeof(dirEntry));

  //Add each directory entry from the array into the hash table
  int i = 0;
  dirEntry* currDirEntry = malloc(sizeof(dirEntry));
  currDirEntry = &arr[0];

  while (strcmp(currDirEntry->filename, "") != 0) {
    setEntry(currDirEntry->filename, currDirEntry, dirPtr);
    i++;
    currDirEntry = &arr[i];
  }

  return dirPtr;
}

//Initialize the file system
int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize) {
  printf("Initializing File System with %ld blocks with a block size of %ld\n",
    numberOfBlocks, blockSize);

  blockSize = blockSize;
  struct volumeCtrlBlock* vcbPtr = malloc(blockSize);

  // Reads data into VCB to check signature
  LBAread(vcbPtr, 1, 0);

  if (vcbPtr->signature == SIG) {
    //Volume was already formatted
    printf("Volume is formatted!\n");
  } else {
    //Volume was not properly formatted

    vcbPtr->signature = SIG;
    vcbPtr->blockSize = blockSize;
    vcbPtr->blockCount = numberOfBlocks;
    vcbPtr->freeBlockNum = FREE_SPACE_START_BLOCK;


    // We will be dealing with free space using 32 bits at a time
    // represented by 1 int that's why we need to determine how
    // many such ints we need, so we need: 19531 / 32 = 610 + 1 = 611 
    // ints, because 611 * 32 = 19552 bits which are enough to
    // represent 19531 blocks. The reason why we add 1 to the 610
    // is because 610 * 32 = 19520 bits which are not enough to
    // represent 19531 blocks
    int numOfInts = (numberOfBlocks / 32) + 1;

    // Test
    printf("Number of ints: %d\n", numOfInts);

    // Since we can only read and write data to and from LBA in
    // blocks we need to malloc memory for our bitVector in
    // block sizes as well
    int* bitVector = malloc(5 * blockSize);

    // 0 = occupied
    // 1 = free

    // Set first 6 bits to 0 and the rest of 25 bits of 1st integer to 1, 
    // because block 0 of LBA is the VCB, and 1 to 5 blocks will be taken 
    // by the bitVector itself
    int totalBits = 0;
    for (int i = 31; i >= 0; i--) {
      totalBits++;
      if (i >= 26) {
        // Set bit to 0
        bitVector[0] = bitVector[0] & ~(1 << i);
      } else {
        // Set bit to 1
        bitVector[0] = bitVector[0] | (1 << i);
      }
    }

    // Set all the bits starting from bit 33 to 1
    for (int i = 1; i < numOfInts; i++) {
      for (int j = 31; j >= 0; j--) {

        // Set bit to 1
        bitVector[i] = bitVector[i] | (1 << j);
      }
    }

    // Saves starting block of the free space and root directory in the VCB
    int numBlocksWritten = LBAwrite(bitVector, 5, FREE_SPACE_START_BLOCK);

    vcbPtr->freeBlockNum = FREE_SPACE_START_BLOCK;
    vcbPtr->rootDir = getFreeBlockNum(numOfInts, bitVector);
    printf("Root directory starts @: %d block\n", vcbPtr->rootDir);

    int sizeOfEntry = sizeof(dirEntry);	//48 bytes
    int dirSize = (5 * blockSize);	//2560 bytes
    int numofEntries = dirSize / sizeOfEntry; //53 entries

    // Initialize our root directory to be a new hash table of directory entries
    rootDir = hashTableInit(numofEntries);

    // Initializing the "." current directory and the ".." parent Directory 
    dirEntry* curDir = dirEntryInit(".", 1, FREE_SPACE_START_BLOCK + numBlocksWritten,
      numofEntries, time(0), time(0));
    setEntry(curDir->filename, curDir, rootDir);

    dirEntry* parentDir = dirEntryInit("..", 1, FREE_SPACE_START_BLOCK +
      numBlocksWritten, numofEntries, time(0), time(0));
    setEntry(parentDir->filename, parentDir, rootDir);

    // dirEntry* testDir = dirEntryInit("..*", 1, FREE_SPACE_START_BLOCK +
    //   numBlocksWritten, numofEntries, time(0), time(0));
    // setEntry(testDir->filename, testDir, dirEntries);

    // Writes VCB to block 0
    int writeVCB = LBAwrite(vcbPtr, 1, 0);

    //Get the number of the next free block
    int freeBlock = getFreeBlockNum(numOfInts, bitVector);
    printf("Mid --> Free block starts at @: %d block\n", freeBlock);

    //Set the allocated blocks to 0 and the directory entry data 
    //stored in the hash table
    setBlocksAsAllocated(freeBlock, 5, bitVector);
    writeTableData(rootDir, 5, freeBlock - 1, blockSize);

    //Update the bitvector
    LBAwrite(bitVector, 5, 1);

  }

  return 0;
}


void exitFileSystem() {
  printf("System exiting\n");
}

//Check is a path is a directory (1 = yes, 0 = no)
int fs_isDir(char* path) {
  //Parse path
  char** pathParts;

  //Traverse the path one component at a time starting from the root directory
  hashTable* currDir = rootDir;
  char* nextDirName = pathParts[0];
  int i = 1;

  //Continue until we have processed each component in the path
  while (strcmp(nextDirName, "\0") != 0) {

    //check that the location exists and that it is a directory
    dirEntry* entry = getEntry(nextDirName, currDir);
    if (entry == NULL || entry->isDir == 0) {
      return 0;
    }

    //Move the current directory to the current component's directory
    //now that it has been verified
    currDir = readTableData(5, entry->location, blockSize);

    //Move on to the next component of the path
    nextDirName = pathParts[i];
  }

  return 1;
}

//Check is a path is a file (1 = yes, 0 = no)
int fs_isFile(char* path) {
  return !fs_isDir(path);
}

// Helper functions
char** stringParser(char* stringToParse) {
  // Divide the path provided by the user into
  // several sub paths
  char ** subStrings = malloc(100);
  char *subString;
  char *savePtr;
  char *delim = "/";

  int stringCount = 0;
  subString = strtok_r(stringToParse, delim, &savePtr);

  while (subString != NULL) {
    subStrings[stringCount] = subString;
    stringCount++;
    printf("Directory entry is: %s\n", subString);
    subString = strtok_r(NULL, delim, &savePtr);
  }

  subStrings[stringCount] = subString;

  
  return subStrings;
}

// Implementation of directory functions

int fs_mkdir(const char* pathname, mode_t mode) {
  puts(pathname);

  char ** dirEnt = stringParser((char *)pathname);

  for (int i = 0; dirEnt[i] != NULL; i++) {
    printf("In the fs_mkdir, directory entry is: %s\n", dirEnt[i]);
  }

  // Reads data into VCB
  struct volumeCtrlBlock* vcbPtr = malloc(512);
  LBAread(vcbPtr, 1, 0);

  // Get the root directory out of the volume and into
  // memory
  dirEntry* rootDir = malloc(5 * 512);
  LBAread(rootDir, 5, 6);

  int sizeOfEntry = sizeof(dirEntry);	//48 bytes
  int dirSize = (5 * 512);	//2560 bytes
  int numofEntries = dirSize / sizeOfEntry; //53 entries


  // Get the bitVector in memory -- We need to know what
  // block is free so we can store our new directory
  // there
  int* bitVector = malloc(5 * 512);

  // Read the bitvector
  LBAread(bitVector, 5, 1);

  for (int i = 0; i < SIZE; i++) {
    int index = i;
    if (strcmp(rootDir[index].filename, "") == 0) {
      int freeBlock = getFreeBlockNum(611, bitVector);

      // Create home directory entry
      strcpy(rootDir[index].filename, "home");
      rootDir[index].isDir = 1;
      rootDir[index].location = freeBlock;
      rootDir[index].fileSize = 5 * 512;
      rootDir[index].dateModified = time(0);
      rootDir[index].dateCreated = time(0);

      // Initialize the home directory
      // Points to an array of directory entries in a free state
      hashTable* dirEntries = hashTableInit(numofEntries);

      // Initializing the "." current directory and the ".." parent Directory 
      dirEntry* curDir = dirEntryInit(".", 1, freeBlock,
        numofEntries, time(0), time(0));
      setEntry(curDir->filename, curDir, dirEntries);

      dirEntry* parentDir = dirEntryInit("..", 1, freeBlock,
        numofEntries, time(0), time(0));
      setEntry(parentDir->filename, parentDir, dirEntries);

      // Write the updated root directory to contain the home
      // directory entry
      LBAwrite(rootDir, 5, 6);

      printf("Free block found at: %d\n", freeBlock);
      writeTableData(dirEntries, 5, freeBlock, 512);

      // Update the bit vector
      setBlocksAsAllocated(freeBlock, 5, bitVector);
      LBAwrite(bitVector, 5, 1);
      break;
    }


  }

  //int index = hash("home");

  // if (strcmp(rootDir[index].filename, "") == 0) {
  //     // dirEntry* testDir = dirEntryInit("home", 1, 6, 
  //     // numofEntries, time(0), time(0));

  //     strcpy(rootDir[index].filename, "home");
  //     rootDir[index].isDir = 1;
  //     rootDir[index].location = 11;
  //     rootDir[index].fileSize = 5 * 512;
  //     rootDir[index].dateModified = time(0);
  //     rootDir[index].dateCreated = time(0);

  //     LBAwrite(rootDir, 5, 6);
  // }




  // Create and initialize a new directory as a directory
  // entry within the root directory
  // int sizeOfEntry = sizeof(dirEntry);	//48 bytes
  // int dirSize = (5 * 512);	//2560 bytes
  // int numofEntries = dirSize / sizeOfEntry; //53 entries

  // dirEntry* curDir = dirEntryInit(".", 1, vcbPtr->rootDir,
  // numofEntries, time(0), time(0));
  // setEntry(curDir->filename, curDir, rootDir);

  // dirEntry* parentDir = dirEntryInit("..", 1, FREE_SPACE_START_BLOCK +
  // numBlocksWritten, numofEntries, time(0), time(0));
  // setEntry(parentDir->filename, parentDir, dirEntries);

  // Write the updated root directory to the volume

  return 0001;
}

/**************************************************************
* Class:  CSC-415-02 Fall 2021
* Names: Patrick Celedio, Chase Alexander, Gurinder Singh, Jonathan Luu
* Student IDs: 920457223, 921040156, 921369355, 918548844
* GitHub Name: csc415-filesystem-CalDevC
* Group Name: Sudoers
* Project: Basic File System
*
* File: directory.c
*
* Description: This file holds the implementations of our
* function to initialize a directory entry and the functions
* related to our hash table.
*
**************************************************************/

#include "directory.h"

//Initialize a new directory entry
dirEntry* dirEntryInit(char filename[20], int isDir, int location,
  unsigned int fileSize, time_t dateModified, time_t dateCreated) {

  dirEntry* entry = malloc(sizeof(dirEntry));

  strcpy(entry->filename, filename);
  entry->isDir = isDir;
  entry->location = location;
  entry->fileSize = fileSize;
  entry->dateModified = dateModified;
  entry->dateCreated = dateCreated;

  return entry;
}

//Get the hash value for a given key (filenames are used as keys)
int hash(const char filename[20]) {
  int value = 1;

  //Alter the hash value based on each char in 
  //the name to try to get a unique value
  int length = strlen(filename);

  for (int i = 0; i < length; i++) {
    value *= 2 + filename[i];
  }

  //If the integer overflows, correct the value to be positive
  if (value < 0) {
    value *= -1;
  }

  //Return a value that will definitely be a valid index in the table
  //by using the remainder of (calculated value / table size)
  return value % SIZE;
}

//Initialize an entry for the hash table
node* entryInit(char key[20], dirEntry* value) {
  //allocate memory for the entry in the table and 
  //the entry's value (directory entry)
  node* entry = malloc(sizeof(node));
  entry->value = malloc(sizeof(dirEntry));

  //Transfer the data to the allocated memory
  strcpy(entry->key, key);
  memcpy(entry->value, value, sizeof(dirEntry));
  entry->next = NULL;

  return entry;
}

//Initialize a new hashTable
hashTable* hashTableInit(char* dirName, int maxNumEntries, int location) {
  hashTable* table = malloc(sizeof(node) * SIZE);
  table->maxNumEntries = maxNumEntries;
  table->location = location;
  strcpy(table->dirName, dirName);

  //Each node in the table should be set to a directory entry with a filename
  //of "" so that we know if there is a collision or not
  for (int i = 0; i < SIZE; i++) {
    dirEntry* entryVal = dirEntryInit("", 0, 0, 0, time(NULL), time(NULL));
    node* entry = entryInit("", entryVal);
    table->entries[i] = entry;
  }

  table->numEntries = 0;

  return table;
}

//Update an existing entry or add a new one
void setEntry(char key[20], dirEntry* value, hashTable* table) {
  //Get the entry based on the hash value calculated from the key
  int hashVal = hash(key);
  node* entry = table->entries[hashVal];

  //If we have already reached the maximum capacity of the directory,
  //Don't attempt to create another directory entry
  if (table->numEntries == table->maxNumEntries) {
    printf("Directory is full, no directory entry created.\n");
    return;
  }

  //If there is no collision then add a new initialized entry
  if (strcmp(entry->value->filename, "") == 0) {
    table->entries[hashVal] = entryInit(key, value);
    table->numEntries++;
    return;
  }

  node* prevEntry;

  //If there is a collision
  while (entry != NULL) {

    //If the current entry has the same key that we are attempting 
    //to add then update the existing entry
    if (strcmp(entry->key, key) == 0) {
      free(entry->value);
      entry->value = malloc(sizeof(dirEntry));
      memcpy(entry->value, value, sizeof(dirEntry));
      return;
    }

    //Move on to check the next entry at the current table location
    prevEntry = entry;
    entry = prevEntry->next;
  }

  //If the key was not found at the table location then add it to 
  //the end of the list at that location
  prevEntry->next = entryInit(key, value);
  table->numEntries++;

}

//Retrieve an entry from a provided hashTable
dirEntry* getEntry(char key[20], hashTable* table) {
  //Get the entry based on the hash value calculated from the key
  int hashVal = hash(key);
  node* entry = table->entries[hashVal];

  //Checks hashTable for matching key and return its value
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return entry->value;
    }
    entry = entry->next;
  }

  return NULL;
}

//Remove an existing entry
// void rmEntry(char key[20], hashTable* table) {
//   //Get the entry based on the hash value calculated from the key
//   int hashVal = hash(key);
//   node* entry = table->entries[hashVal];

//     node* prevEntry;

//   //If there is a collision
//   while (entry != NULL) {

//     //If the current entry has the same key that we are attempting 
//     //to remove then update the existing entry
//     if (strcmp(entry->key, key) == 0) {
//       free(entry->value);
//       entry->value = malloc(sizeof(dirEntry));
//       memcpy(entry->value, value, sizeof(dirEntry));
//       prevEntry->next = entry->next;
//       table->numEntries--;
//       return;
//     }

//     //Move on to check the next entry at the current table location
//     prevEntry = entry;
//     entry = prevEntry->next;
//   }

//   //If the key was not found at the table location then add it to 
//   //the end of the list at that location
//   prevEntry->next = entry->next;
//   table->numEntries--;
// }

//Given an index, find the index of the next entry in the table
int getNextIdx(int currIdx, hashTable* table) {
  //If we are looking for the first index in the list, start at entry 0
  int max = table->maxNumEntries;

  if (currIdx == max && strcmp(table->entries[0]->key, "") != 0) {
    return 0;
  } else if (currIdx == max) {
    currIdx = 0;
  }

  node* currNode = table->entries[currIdx];

  //Return the same index is there is another element hashed to that location
  if (currNode->next != NULL) {
    return currIdx;
  }

  //Otherwise continue through the table looking for the next non-free entry
  for (int i = currIdx + 1; i < SIZE; i++) {
    currNode = table->entries[i];
    // printf("index: %d, currNode filename: %s\n", i, currNode->value->filename);
    if (strcmp(currNode->value->filename, "") != 0) {
      return i;
    }
  }

  return max;
}

//Write out the hash table contents to the console for debug
void printTable(hashTable* table) {
  printf("\n******** table ********\n");
  for (int i = 0; i < SIZE; i++) {
    node* entry = table->entries[i];
    if (strcmp(entry->value->filename, "") != 0) {
      printf("[Entry %d] %s", i, table->entries[i]->key);
      while (entry->next != NULL) {
        entry = entry->next;
        printf(", %s", entry->key);
      }
      printf("\n");
    }
  }

}

//Free the memory allocated to the hashTable
void clean(hashTable* table) {
  for (int i = 0; i < SIZE; i++) {
    node* entry = table->entries[i];
    free(entry);
  }
  free(table);
}
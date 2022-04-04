#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#define ENTRIES_PER_BLOCK 16

typedef struct dirEntry {
  int location;           //The block number where the start of the file is stored
  char filename[20];      //The name of the file (provided by creator)
  unsigned int fileSize;  //Length of file in bytes
  time_t dateModified;    //Date file was last modified
  time_t dateCreated;	    //Date file was created
} dirEntry;

//Initialize a new directory entry
dirEntry* dirEntryInit(char filename[20], int location, unsigned int fileSize,
  time_t dateModified, time_t dateCreated) {

  dirEntry* entry = malloc(sizeof(dirEntry));

  strcpy(entry->filename, filename);
  entry->location = location;
  entry->fileSize = fileSize;
  entry->dateModified = dateModified;
  entry->dateCreated = dateCreated;

  return entry;
}

// *************************** Hashmap functions *************************** //
#define SIZE 1000  //Number of positions in the hashmap

//Node objects are used to populate the hasmap
typedef struct node {
  char key[20];       //The filename will be used as the key
  dirEntry* value;    //The directory entry associated with the filename
  struct node* next;  //points to the next object at that map position
} node;

//Hashmap object that holds all of the node entries
typedef struct hashmap {
  node* entries[SIZE];
  int numEntries;
} hashmap;

//Initialize a new hashmap
hashmap* hashmapInit() {
  hashmap* newMap = malloc(sizeof(node*) * SIZE);

  //Each node in the map should be set to NULL so that we know if there 
  //is a collision or not
  for (int i = 0; i < SIZE; i++) {
    node* temp = NULL;
    newMap->entries[i] = temp;
  }

  newMap->numEntries = 0;

  return newMap;
}

//Get the hash value for a given key (filenames are used as keys)
int hash(const char filename[20]) {
  int value = 1;

  //Alter the hash value based on each char in 
  //the name to try to get a unique value
  for (int i = 0; i < 20; i++) {
    value *= 2 + filename[i];
  }

  //If the integer overflows, correct the value to be positive
  if (value < 0) {
    value *= -1;
  }

  //Return a value that will definitely be a valid index in the map
  //by using the remainder of (calculated value / map size)
  return value % SIZE;
}

//Initialize an entry for the hashmap
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

//Update an existing entry or add a new one
void setEntry(char key[20], dirEntry* value, hashmap* map) {
  //Get the entry based on the hash value calculated from the key
  int hashVal = hash(key);
  node* entry = map->entries[hashVal];
  map->numEntries++;

  //If there is no collision then add a new initialized entry
  if (entry == NULL) {
    map->entries[hashVal] = entryInit(key, value);
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

    //Move on to check the next entry at the current map location
    prevEntry = entry;
    entry = prevEntry->next;
  }

  //If the key was not found at the map location then add it to 
  //the end of the list at that location
  prevEntry->next = entryInit(key, value);

}

//Retrieve an entry from a provided hashmap
dirEntry* getEntry(char key[20], hashmap* map) {
  //Get the entry based on the hash value calculated from the key
  int hashVal = hash(key);
  node* entry = map->entries[hashVal];

  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return entry->value;
    }

    entry = entry->next;
  }

  return NULL;
}

void printMap(hashmap* map) {
  printf("******** MAP ********");
  for (int i = 0; i < SIZE; i++) {
    node* entry = map->entries[i];
    if (entry != NULL) {
      printf("\n[Entry %d] %s", i, map->entries[i]->key);
      while (entry->next != NULL) {
        entry = entry->next;
        printf(", %s", entry->key);
      }
    }
  }

}

void writeMapData(hashmap* map, int lbaCount, int lbaPosition) {
  //Create an array whose size is the number of directory entries in map
  int arrSize = map->numEntries;
  dirEntry arr[arrSize];

  //j will track indcies for the array
  int j = 0;

  //iterate through the whole map
  for (int i = 0; i < SIZE; i++) {
    node* entry = map->entries[i];
    if (entry != NULL) {
      arr[j] = *entry->value;  //add entry
      j++;

      //add other entries that are at the same hash location
      while (entry->next != NULL) {
        entry = entry->next;
        arr[j] = *entry->value;  //add entry
        j++;
      }
    }

    //Don't bother lookng through rest of map if all entries are found
    if (j == arrSize - 1) {
      break;
    }
  }

  //Write the array to the disk
  LBAwrite(arr, lbaCount, lbaPosition);
}

//Free the memory allocated to the hashmap
void clean(hashmap* map) {
  for (int i = 0; i < SIZE; i++) {
    node* entry = map->entries[i];
    free(entry);
  }
  free(map);
}

int main() {
  hashmap* map = hashmapInit();
  time_t testTime = time(NULL);

  dirEntry* entry1 = dirEntryInit("myFile", 12, 10, testTime, testTime);
  setEntry("myFile", entry1, map);

  dirEntry* entry2 = dirEntryInit("myFileTest", 12, 10, testTime, testTime);
  setEntry(entry2->filename, entry2, map);

  dirEntry* entry3 = dirEntryInit("myOtherFile", 12, 10, testTime, testTime);
  setEntry(entry3->filename, entry3, map);

  printMap(map);
  printf("\n\nReleasing memory\n");
  clean(map);


  // printf("%ld\n%d\n", time(0), time(NULL));
  printf("%ld -> %ld, %ld, %ld, %ld, %ld\n", sizeof(dirEntry), sizeof(int), sizeof(char[20]), sizeof(unsigned int), sizeof(time_t), sizeof(char*));
}
// OpenRelTable.cpp
#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTable::OpenRelTable() {

// initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
  }
//RELATIONS CACHE
//-------Fetch RelCatBlock from buffer---------//

  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];

  //RELATION CATALOG TO REL CACHE
  // 1. Relcat Block --> Relcat Record ([0])
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);
  
  // 2. Convert record to cache (just convert and store it in relcachentry)
  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  // 3. point it 
  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry)); //allocate memory and points it
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry; // copies to memory address



  //ATTRIBUTE CATALOG TO REL CACHE
  //1. Relcat Block --> AttrCat Record ([1])
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  //2. Convert record to cache (just convert and store it in relcachentry)
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  // 3. point it 
  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry)); //allocate memory and points it
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry; // copies to memory address




//ATTRIBUTES CACHE
//-------Fetch AttrCatBlock from buffer---------//

  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  //RELATION CATALOG TO ATTR CACHE

  //1. Run loop from 0 -> Number of attributes of Relcat    
  //2. Get record of i
  //3. Convert record to cache (just convert and store it in relcachentry)
  //4. point it 
  //5. make linked list

  AttrCacheEntry* head = nullptr;
  AttrCacheEntry* prev = nullptr;
  //1
  for(int i = 0; i < NO_OF_ATTRS_RELCAT_ATTRCAT; ++i){

    //2
    attrCatBlock.getRecord(attrCatRecord, i);

    //3
    struct AttrCacheEntry attrCacheEntry;
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry.attrCatEntry);
    attrCacheEntry.recId.block = ATTRCAT_BLOCK;
    attrCacheEntry.recId.slot = i;  
    attrCacheEntry.next = nullptr;

    //4
    AttrCacheEntry* curr = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry)); //allocate memory and points it
    *curr = attrCacheEntry;

    //5
    if(i == 0) {
      head = curr;
    }else{
      prev->next = curr;
    }
    prev = curr;
  }


  AttrCacheTable::attrCache[RELCAT_RELID] = head;
  head = nullptr;
  prev = nullptr;

  for(int i = 0; i < NO_OF_ATTRS_RELCAT_ATTRCAT; ++i){
    int slotNum = i + NO_OF_ATTRS_RELCAT_ATTRCAT;
    attrCatBlock.getRecord(attrCatRecord, slotNum);

    struct AttrCacheEntry attrCacheEntry;
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry.attrCatEntry);
    attrCacheEntry.recId.block = ATTRCAT_BLOCK;
    attrCacheEntry.recId.slot = slotNum;  
    attrCacheEntry.next = nullptr;
    AttrCacheEntry* curr = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry)); //allocate memory and points it
    *curr = attrCacheEntry;
    if(i == 0) {
      head = curr;
    }else{
      prev->next = curr;
    }
    prev = curr;
  }
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;


}
OpenRelTable::~OpenRelTable() {

  // free relation cache
  for (int i = 0; i < MAX_OPEN; ++i) {
    if (RelCacheTable::relCache[i] != nullptr) {
      free(RelCacheTable::relCache[i]);
      RelCacheTable::relCache[i] = nullptr;
    }
  }

  // free attribute cache (linked lists)
  for (int i = 0; i < MAX_OPEN; ++i) {
    AttrCacheEntry* curr = AttrCacheTable::attrCache[i];
    while (curr != nullptr) {
      AttrCacheEntry* next = curr->next;
      free(curr);
      curr = next;
    }
    AttrCacheTable::attrCache[i] = nullptr;
  }
}
/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
    if (strcmp(relName, RELCAT_RELNAME) == 0)
        return RELCAT_RELID;

    if (strcmp(relName, ATTRCAT_RELNAME) == 0)
        return ATTRCAT_RELID;

    return E_RELNOTOPEN;
}

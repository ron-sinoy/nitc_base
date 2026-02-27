// OpenRelTable.cpp
#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

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

//Initialize metainfo table
    for(int i=0; i<MAX_OPEN; i++)
        tableMetaInfo[i].free = true;

    OpenRelTable::tableMetaInfo[RELCAT_RELID].free = false;
    OpenRelTable::tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(OpenRelTable::tableMetaInfo[RELCAT_RELID].relName,RELCAT_RELNAME);
    strcpy(OpenRelTable::tableMetaInfo[ATTRCAT_RELID].relName,ATTRCAT_RELNAME);


  }
OpenRelTable::~OpenRelTable() {

    for (int i = 2; i < MAX_OPEN; ++i) {
    if (!tableMetaInfo[i].free) {
      OpenRelTable::closeRel(i); // we will implement this function later
    }
    }


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


int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

  for(int i = 0; i<MAX_OPEN; ++i){
    if(!tableMetaInfo[i].free && strcmp(relName, tableMetaInfo[i].relName)==0){
      return i;
    } 
  }
  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {

  for(int i = 0; i<MAX_OPEN; ++i){
    if(tableMetaInfo[i].free) return i;
  }
  return E_CACHEFULL;

}


int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
  int relId;
  relId = OpenRelTable::getRelId(relName);
  if(relId>=0){
    return relId;
  }
  relId = OpenRelTable::getFreeOpenRelTableEntry();

  if (relId<0){
    return E_CACHEFULL;
  }


  /****** Setting up Relation Cache entry for the relation ******/
  Attribute attrVal;
  strcpy(attrVal.sVal, relName);
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, "RelName", attrVal, EQ);
  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      Care should be taken to reset the searchIndex of the relation RELCAT_RELID
      before calling linearSearch().*/

  // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
  if ( relcatRecId.block == -1 || relcatRecId.slot == -1 ) {
    // (the relation is not found in the Relation Catalog.)
    return E_RELNOTEXIST;
  }


  RecBuffer recBuffer(relcatRecId.block);
  Attribute rec[RELCAT_NO_ATTRS];
  recBuffer.getRecord(rec,relcatRecId.slot);
  RelCacheEntry *relCacheEntry = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  RelCacheTable::recordToRelCatEntry(rec,&relCacheEntry->relCatEntry);
  relCacheEntry->recId = relcatRecId;
  relCacheEntry->searchIndex = {-1, -1};  // initialize search index

  RelCacheTable::relCache[relId] = relCacheEntry;

  /* read the record entry corresponding to relcatRecId and create a relCacheEntry
      on it using RecBuffer::getRecord() and RelCacheTable::recordToRelCatEntry().
      update the recId field of this Relation Cache entry to relcatRecId.
      use the Relation Cache entry to set the relId-th entry of the RelCacheTable.
    NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
  */

  /****** Setting up Attribute Cache entry for the relation ******/

AttrCacheEntry *listHead = nullptr;

RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  
RecId attrcatRecId;
while ((attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, "RelName", attrVal, EQ)).block != -1) {

  RecBuffer recBuffer(attrcatRecId.block);
  Attribute rec[ATTRCAT_NO_ATTRS];
  recBuffer.getRecord(rec, attrcatRecId.slot);

  AttrCacheEntry *newEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
  AttrCacheTable::recordToAttrCatEntry(rec, &newEntry->attrCatEntry);
  newEntry->recId = attrcatRecId;
  newEntry->searchIndex = {-1, -1};

  // attach existing list to new node, then make new node the head
  newEntry->next = listHead;
  listHead = newEntry;
}

AttrCacheTable::attrCache[relId] = listHead;

/****** Setting up metadata in the Open Relation Table for the relation ******/

tableMetaInfo[relId].free = false;
strcpy(tableMetaInfo[relId].relName, relName);

return relId;
}

int OpenRelTable::closeRel(int relId) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }

  // free relation cache
  free(RelCacheTable::relCache[relId]);
  RelCacheTable::relCache[relId] = nullptr;

  // free attribute cache linked list
  AttrCacheEntry* curr = AttrCacheTable::attrCache[relId];
  while (curr != nullptr) {
    AttrCacheEntry* next = curr->next;
    free(curr);
    curr = next;
  }
  AttrCacheTable::attrCache[relId] = nullptr;

  // mark slot as free
  tableMetaInfo[relId].free = true;

  return SUCCESS;
}
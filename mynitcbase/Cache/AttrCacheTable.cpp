#include "AttrCacheTable.h"

#include <cstring>
AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];


int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {
    if( relId <0 || relId>=12 ){
        return E_OUTOFBOUND;
    }
    if (relCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

  // traverse the linked list of attribute cache entries
  for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
    if (entry->attrCatEntry.offset == attrOffset) {
      *attrCatBuf = entry->attrCatEntry;
      return SUCCESS;
    }
  }

  // there is no attribute at this offset
  return E_ATTRNOTEXIST;
}


void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS],
                                          AttrCatEntry* attrCatEntry) {
  strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
  attrCatEntry->numAttrs = record[ATTRCAT_NO_ATTRIBUTES_INDEX].nVal;
  attrCatEntry->numRecs = record[ATTRCAT_NO_RECORDS_INDEX].nVal;
  attrCatEntry->firstBlk = record[ATTRCAT_FIRST_BLOCK_INDEX].nVal;
  attrCatEntry->lastBlk = record[ATTRCAT_LAST_BLOCK_INDEX].nVal;
  attrCatEntry->numSlotsPerBlk = record[ATTRCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}
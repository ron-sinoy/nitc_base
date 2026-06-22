#include "AttrCacheTable.h"

#include <cstring>
AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

namespace {
AttrCacheEntry *findAttrEntryByName(AttrCacheEntry *head, const char attrName[ATTR_SIZE]) {
  for (AttrCacheEntry *entry = head; entry != nullptr; entry = entry->next) {
    if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
      return entry;
    }
  }
  return nullptr;
}

AttrCacheEntry *findAttrEntryByOffset(AttrCacheEntry *head, int attrOffset) {
  for (AttrCacheEntry *entry = head; entry != nullptr; entry = entry->next) {
    if (entry->attrCatEntry.offset == attrOffset) {
      return entry;
    }
  }
  return nullptr;
}
}  // namespace


int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByOffset(attrCache[relId], attrOffset);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  *attrCatBuf = entry->attrCatEntry;
  return SUCCESS;
}

int AttrCacheTable::getAttrCatEntry(int relId,char attrName[ATTR_SIZE],AttrCatEntry* attrCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByName(attrCache[relId], attrName);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  *attrCatBuf = entry->attrCatEntry;
  return SUCCESS;
}

int AttrCacheTable::setAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByName(attrCache[relId], attrName);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  entry->attrCatEntry = *attrCatBuf;
  entry->dirty = true;
  return SUCCESS;
}

int AttrCacheTable::setAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByOffset(attrCache[relId], attrOffset);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  entry->attrCatEntry = *attrCatBuf;
  entry->dirty = true;
  return SUCCESS;
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry,
                                          union Attribute record[ATTRCAT_NO_ATTRS]) {
  strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
  strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);
  record[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrCatEntry->attrType;
  record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = attrCatEntry->primaryFlag;
  record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = attrCatEntry->rootBlock;
  record[ATTRCAT_OFFSET_INDEX].nVal = attrCatEntry->offset;
}

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS],
                                          AttrCatEntry* attrCatEntry) {
  strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
  strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
  attrCatEntry->attrType = record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
  attrCatEntry->primaryFlag = record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
  attrCatEntry->rootBlock = record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
  attrCatEntry->offset = record[ATTRCAT_OFFSET_INDEX].nVal;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByName(attrCache[relId], attrName);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  *searchIndex = entry->searchIndex;
  return SUCCESS;
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByOffset(attrCache[relId], attrOffset);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  *searchIndex = entry->searchIndex;
  return SUCCESS;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByName(attrCache[relId], attrName);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  entry->searchIndex = *searchIndex;
  return SUCCESS;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  AttrCacheEntry *entry = findAttrEntryByOffset(attrCache[relId], attrOffset);
  if (entry == nullptr) {
    return E_ATTRNOTEXIST;
  }

  entry->searchIndex = *searchIndex;
  return SUCCESS;
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]) {
  IndexId searchIndex{-1, -1};
  return setSearchIndex(relId, attrName, &searchIndex);
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset) {
  IndexId searchIndex{-1, -1};
  return setSearchIndex(relId, attrOffset, &searchIndex);
}



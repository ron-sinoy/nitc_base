#include "Schema.h"

#include <cmath>
#include <cstring>
#include <iostream>

int Schema::createRel(char relName[], int numOfAttributes, char attrNames[][ATTR_SIZE], int attrType[]) {
  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);

  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  RecId relRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
  if (relRecId.block != -1 || relRecId.slot != -1) {
    return E_RELEXIST;
  }

  for (int i = 0; i < numOfAttributes; ++i) {
    for (int j = i + 1; j < numOfAttributes; ++j) {
      if (strcmp(attrNames[i], attrNames[j]) == 0) {
        return E_DUPLICATEATTR;
      }
    }
  }

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = numOfAttributes;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal =
      floor((BLOCK_SIZE - HEADER_SIZE) / (ATTR_SIZE * numOfAttributes + 1));

  int ret = BlockAccess::insert(RELCAT_RELID, relCatRecord);
  if (ret != SUCCESS) {
    return ret;
  }

  for (int i = 0; i < numOfAttributes; ++i) {
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrNames[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrType[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    ret = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
    if (ret != SUCCESS) {
      BlockAccess::deleteRelation(relName);
      return ret;
    }
  }

  return SUCCESS;
}

int Schema::deleteRel(char relName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId != E_RELNOTOPEN) {
    return E_RELOPEN;
  }

  return BlockAccess::deleteRelation(relName);
}

int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  if(ret >= 0){
    return SUCCESS;
  }

  return ret;
}

int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return BPlusTree::bPlusCreate(relId, attrName);
}

int Schema::dropIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  if (attrCatEntry.rootBlock == -1) {
    return E_NOINDEX;
  }

  ret = BPlusTree::bPlusDestroy(attrCatEntry.rootBlock);
  if (ret != SUCCESS) {
    return ret;
  }

  attrCatEntry.rootBlock = -1;
  return AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  if (strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not. we will implement this later.
  int relId = OpenRelTable::getRelId(relName);

  if (relId<0) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}
int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
    if(strcmp(oldRelName,RELCAT_RELNAME) == 0 || strcmp(oldRelName,ATTRCAT_RELNAME) == 0 || strcmp(newRelName,RELCAT_RELNAME) == 0 || strcmp(newRelName,ATTRCAT_RELNAME) == 0){
      return E_NOTPERMITTED;
    } 
    int relId = OpenRelTable::getRelId(oldRelName);
    if(relId==E_RELOPEN) return E_RELOPEN;
    int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
    return retVal;
} 
int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {

if(strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0 ){
      return E_NOTPERMITTED;
    } 
    int relId = OpenRelTable::getRelId(relName);
    if(relId==E_RELOPEN) return E_RELOPEN;

    int retVal = BlockAccess::renameAttribute(relName,oldAttrName,newAttrName);
    return retVal;
}

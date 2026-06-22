#include "Algebra.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../BlockAccess/BlockAccess.h"
#include "../Cache/AttrCacheTable.h"

namespace {
bool isNumber(char *str) {
  int len;
  float ignore;
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == (int)strlen(str);
}

int getSourceAttrs(int relId, int numAttrs, char attrNames[][ATTR_SIZE], int attrTypes[]) {
  for (int i = 0; i < numAttrs; ++i) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    if (ret != SUCCESS) {
      return ret;
    }
    strcpy(attrNames[i], attrCatEntry.attrName);
    attrTypes[i] = attrCatEntry.attrType;
  }
  return SUCCESS;
}

int getAttrOffsets(int relId, int attrCount, char attrList[][ATTR_SIZE], int attrOffsets[], int attrTypes[]) {
  for (int i = 0; i < attrCount; ++i) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrList[i], &attrCatEntry);
    if (ret != SUCCESS) {
      return ret;
    }
    attrOffsets[i] = attrCatEntry.offset;
    attrTypes[i] = attrCatEntry.attrType;
  }
  return SUCCESS;
}

int createTargetRel(char targetRel[ATTR_SIZE], int nAttrs, char attrNames[][ATTR_SIZE], int attrTypes[]) {
  int ret = Schema::createRel(targetRel, nAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  ret = Schema::openRel(targetRel);
  if (ret != SUCCESS) {
    Schema::deleteRel(targetRel);
    return ret;
  }

  return SUCCESS;
}
}  // namespace

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op,
                    char strVal[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  AttrCatEntry attrCatEntry;
  ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  ret = AttrCacheTable::resetSearchIndex(srcRelId, attr);
  if (ret != SUCCESS) {
    return ret;
  }

  Attribute attrVal;
  if (attrCatEntry.attrType == NUMBER) {
    if (!isNumber(strVal)) {
      return E_ATTRTYPEMISMATCH;
    }
    attrVal.nVal = atof(strVal);
  } else {
    strcpy(attrVal.sVal, strVal);
  }

  char attrNames[relCatEntry.numAttrs][ATTR_SIZE];
  int attrTypes[relCatEntry.numAttrs];
  ret = getSourceAttrs(srcRelId, relCatEntry.numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  ret = createTargetRel(targetRel, relCatEntry.numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::getRelId(targetRel);
  if (targetRelId == E_RELNOTOPEN) {
    Schema::deleteRel(targetRel);
    return E_RELNOTOPEN;
  }

  RelCacheTable::resetSearchIndex(srcRelId);

  while (true) {
    Attribute record[relCatEntry.numAttrs];
    ret = BlockAccess::search(srcRelId, record, attr, attrVal, op);
    if (ret == E_NOTFOUND) {
      break;
    }
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }

    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  ret = Schema::closeRel(targetRel);
  if (ret != SUCCESS) {
    return ret;
  }

  return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  char attrNames[relCatEntry.numAttrs][ATTR_SIZE];
  int attrTypes[relCatEntry.numAttrs];
  ret = getSourceAttrs(srcRelId, relCatEntry.numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  ret = createTargetRel(targetRel, relCatEntry.numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::getRelId(targetRel);
  if (targetRelId == E_RELNOTOPEN) {
    Schema::deleteRel(targetRel);
    return E_RELNOTOPEN;
  }

  RelCacheTable::resetSearchIndex(srcRelId);

  while (true) {
    Attribute record[relCatEntry.numAttrs];
    ret = BlockAccess::project(srcRelId, record);
    if (ret == E_NOTFOUND) {
      break;
    }
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }

    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  ret = Schema::closeRel(targetRel);
  if (ret != SUCCESS) {
    return ret;
  }

  return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs,
                     char tar_Attrs[][ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  if (tar_nAttrs < 0) {
    return E_INVALID;
  }

  int attrOffsets[tar_nAttrs];
  int attrTypes[tar_nAttrs];
  ret = getAttrOffsets(srcRelId, tar_nAttrs, tar_Attrs, attrOffsets, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  ret = createTargetRel(targetRel, tar_nAttrs, tar_Attrs, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::getRelId(targetRel);
  if (targetRelId == E_RELNOTOPEN) {
    Schema::deleteRel(targetRel);
    return E_RELNOTOPEN;
  }

  RelCacheTable::resetSearchIndex(srcRelId);

  while (true) {
    Attribute sourceRecord[relCatEntry.numAttrs];
    ret = BlockAccess::project(srcRelId, sourceRecord);
    if (ret == E_NOTFOUND) {
      break;
    }
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }

    Attribute projectedRecord[tar_nAttrs];
    for (int i = 0; i < tar_nAttrs; ++i) {
      projectedRecord[i] = sourceRecord[attrOffsets[i]];
    }

    ret = BlockAccess::insert(targetRelId, projectedRecord);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  ret = Schema::closeRel(targetRel);
  if (ret != SUCCESS) {
    return ret;
  }

  return SUCCESS;
}

int Algebra::insert(char relName[ATTR_SIZE], int numberOfAttributes, char record[][ATTR_SIZE]) {
  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  if (numberOfAttributes != relCatEntry.numAttrs) {
    return E_NATTRMISMATCH;
  }

  Attribute attrRecord[relCatEntry.numAttrs];
  for (int i = 0; i < relCatEntry.numAttrs; ++i) {
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    if (ret != SUCCESS) {
      return ret;
    }

    if (attrCatEntry.attrType == NUMBER) {
      if (!isNumber(record[i])) {
        return E_ATTRTYPEMISMATCH;
      }
      attrRecord[i].nVal = atof(record[i]);
    } else {
      strcpy(attrRecord[i].sVal, record[i]);
    }
  }

  return BlockAccess::insert(relId, attrRecord);
}

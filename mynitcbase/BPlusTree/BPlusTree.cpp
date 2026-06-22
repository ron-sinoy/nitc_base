#include "BPlusTree.h"

#include <cstring>

#include "../Cache/AttrCacheTable.h"
#include "../Cache/RelCacheTable.h"

namespace {
void assignIndexAttr(Index *entry, const Attribute &attrVal) {
  entry->attrVal = attrVal;
}

void assignInternalAttr(InternalEntry *entry, const Attribute &attrVal) {
  entry->attrVal = attrVal;
}

int updateParentBlock(int blockNum, int parentBlockNum) {
  if (blockNum == -1) {
    return SUCCESS;
  }

  BlockBuffer childBlock(blockNum);
  HeadInfo childHead;
  int ret = childBlock.getHeader(&childHead);
  if (ret != SUCCESS) {
    return ret;
  }

  childHead.pblock = parentBlockNum;
  return childBlock.setHeader(&childHead);
}
}  // namespace

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return {-1, -1};
  }

  IndexId searchIndex{-1, -1};
  ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
  if (ret != SUCCESS) {
    return {-1, -1};
  }

  int attrType = attrCatEntry.attrType;
  int block = attrCatEntry.rootBlock;
  int index = 0;

  if (searchIndex.block != -1 || searchIndex.index != -1) {
    block = searchIndex.block;
    index = searchIndex.index + 1;

    HeadInfo leafHead;
    ret = IndLeaf(block).getHeader(&leafHead);
    if (ret != SUCCESS) {
      return {-1, -1};
    }

    if (index >= leafHead.numEntries) {
      block = leafHead.rblock;
      index = 0;
      if (block == -1) {
        return {-1, -1};
      }
    }
  } else {
    if (block == -1) {
      return {-1, -1};
    }

    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
      IndInternal intBlock(block);
      HeadInfo intHead;
      ret = intBlock.getHeader(&intHead);
      if (ret != SUCCESS) {
        return {-1, -1};
      }

      InternalEntry internalEntry{};
      int chosen = 0;
      for (int entryNum = 0; entryNum < intHead.numEntries; ++entryNum) {
        ret = intBlock.getEntry(&internalEntry, entryNum);
        if (ret != SUCCESS) {
          return {-1, -1};
        }

        int flag = compareAttrs(internalEntry.attrVal, attrVal, attrType);
        switch (op) {
          case EQ:
          case GE:
            if (flag >= 0) {
              block = internalEntry.lChild;
              chosen = 1;
            }
            break;
          case GT:
            if (flag > 0) {
              block = internalEntry.lChild;
              chosen = 1;
            }
            break;
          case LE:
          case LT:
          case NE:
            block = internalEntry.lChild;
            chosen = 1;
            break;
        }

        if (chosen) {
          break;
        }
      }

      if (!chosen) {
        block = internalEntry.rChild;
      }
    }
  }

  while (block != -1) {
    HeadInfo leafHead;
    ret = IndLeaf(block).getHeader(&leafHead);
    if (ret != SUCCESS) {
      return {-1, -1};
    }

    while (index < leafHead.numEntries) {
      Index leafEntry;
      ret = IndLeaf(block).getEntry(&leafEntry, index);
      if (ret != SUCCESS) {
        return {-1, -1};
      }

      int flag = compareAttrs(leafEntry.attrVal, attrVal, attrType);
      bool found = false;
      bool stop = false;

      switch (op) {
        case EQ:
          if (flag == 0) {
            found = true;
          } else if (flag > 0) {
            stop = true;
          }
          break;
        case LE:
          if (flag <= 0) {
            found = true;
          } else {
            stop = true;
          }
          break;
        case LT:
          if (flag < 0) {
            found = true;
          } else {
            stop = true;
          }
          break;
        case GE:
          if (flag >= 0) {
            found = true;
          }
          break;
        case GT:
          if (flag > 0) {
            found = true;
          }
          break;
        case NE:
          if (flag != 0) {
            found = true;
          }
          break;
      }

      if (found) {
        IndexId currentIndex{block, index};
        AttrCacheTable::setSearchIndex(relId, attrName, &currentIndex);
        return {leafEntry.block, leafEntry.slot};
      }

      if (stop) {
        return {-1, -1};
      }

      ++index;
    }

    if (op == NE) {
      block = leafHead.rblock;
      index = 0;
    } else {
      break;
    }
  }

  return {-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  if (attrCatEntry.rootBlock != -1) {
    return SUCCESS;
  }

  IndLeaf rootBlockBuf;
  int rootBlock = rootBlockBuf.getBlockNum();
  if (rootBlock == E_DISKFULL) {
    return E_DISKFULL;
  }

  attrCatEntry.rootBlock = rootBlock;
  ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  RelCatEntry relCatEntry;
  ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  int block = relCatEntry.firstBlk;
  while (block != -1) {
    RecBuffer recBuffer(block);
    HeadInfo head;
    ret = recBuffer.getHeader(&head);
    if (ret != SUCCESS) {
      return ret;
    }

    unsigned char slotMap[head.numSlots];
    ret = recBuffer.getSlotMap(slotMap);
    if (ret != SUCCESS) {
      return ret;
    }

    for (int slot = 0; slot < head.numSlots; ++slot) {
      if (slotMap[slot] == SLOT_UNOCCUPIED) {
        continue;
      }

      Attribute record[head.numAttrs];
      ret = recBuffer.getRecord(record, slot);
      if (ret != SUCCESS) {
        return ret;
      }

      RecId recId{block, slot};
      ret = BPlusTree::bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);
      if (ret == E_DISKFULL) {
        return E_DISKFULL;
      }
      if (ret != SUCCESS) {
        return ret;
      }
    }

    block = head.rblock;
  }

  return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
  if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  int blockType = StaticBuffer::getStaticBlockType(rootBlockNum);
  if (blockType == IND_LEAF) {
    IndLeaf leaf(rootBlockNum);
    leaf.releaseBlock();
    return SUCCESS;
  }

  if (blockType == IND_INTERNAL) {
    IndInternal internalBlk(rootBlockNum);
    HeadInfo head;
    int ret = internalBlk.getHeader(&head);
    if (ret != SUCCESS) {
      return ret;
    }

    if (head.numEntries > 0) {
      InternalEntry entry{};
      ret = internalBlk.getEntry(&entry, 0);
      if (ret != SUCCESS) {
        return ret;
      }
      ret = bPlusDestroy(entry.lChild);
      if (ret != SUCCESS) {
        return ret;
      }

      for (int i = 0; i < head.numEntries; ++i) {
        ret = internalBlk.getEntry(&entry, i);
        if (ret != SUCCESS) {
          return ret;
        }
        ret = bPlusDestroy(entry.rChild);
        if (ret != SUCCESS) {
          return ret;
        }
      }
    }

    internalBlk.releaseBlock();
    return SUCCESS;
  }

  return E_INVALIDBLOCK;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
  int blockNum = rootBlock;

  while (StaticBuffer::getStaticBlockType(blockNum) == IND_INTERNAL) {
    IndInternal internalBlk(blockNum);
    HeadInfo header;
    int ret = internalBlk.getHeader(&header);
    if (ret != SUCCESS) {
      return blockNum;
    }

    if (header.numEntries == 0) {
      return blockNum;
    }

    InternalEntry entry{};
    bool movedLeft = false;
    for (int i = 0; i < header.numEntries; ++i) {
      ret = internalBlk.getEntry(&entry, i);
      if (ret != SUCCESS) {
        return blockNum;
      }

      if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0) {
        blockNum = entry.lChild;
        movedLeft = true;
        break;
      }
    }

    if (!movedLeft) {
      ret = internalBlk.getEntry(&entry, header.numEntries - 1);
      if (ret != SUCCESS) {
        return blockNum;
      }
      blockNum = entry.rChild;
    }
  }

  return blockNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  IndInternal newRootBlk;
  int newRootBlkNum = newRootBlk.getBlockNum();
  if (newRootBlkNum == E_DISKFULL) {
    ret = bPlusDestroy(rChild);
    if (ret != SUCCESS) {
      return ret;
    }
    return E_DISKFULL;
  }

  HeadInfo head;
  ret = newRootBlk.getHeader(&head);
  if (ret != SUCCESS) {
    return ret;
  }
  head.numEntries = 1;
  head.pblock = -1;
  ret = newRootBlk.setHeader(&head);
  if (ret != SUCCESS) {
    return ret;
  }

  InternalEntry entry{};
  entry.lChild = lChild;
  assignInternalAttr(&entry, attrVal);
  entry.rChild = rChild;
  ret = newRootBlk.setEntry(&entry, 0);
  if (ret != SUCCESS) {
    return ret;
  }

  ret = updateParentBlock(lChild, newRootBlkNum);
  if (ret != SUCCESS) {
    return ret;
  }
  ret = updateParentBlock(rChild, newRootBlkNum);
  if (ret != SUCCESS) {
    return ret;
  }

  attrCatEntry.rootBlock = newRootBlkNum;
  return AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
  IndLeaf rightBlk;
  IndLeaf leftBlk(leafBlockNum);

  int rightBlkNum = rightBlk.getBlockNum();
  if (rightBlkNum == E_DISKFULL) {
    return E_DISKFULL;
  }

  HeadInfo leftHead;
  HeadInfo rightHead;
  int ret = leftBlk.getHeader(&leftHead);
  if (ret != SUCCESS) {
    return ret;
  }
  ret = rightBlk.getHeader(&rightHead);
  if (ret != SUCCESS) {
    return ret;
  }

  int oldRightBlock = leftHead.rblock;

  leftHead.numEntries = MIDDLE_INDEX_LEAF + 1;
  leftHead.rblock = rightBlkNum;
  ret = leftBlk.setHeader(&leftHead);
  if (ret != SUCCESS) {
    return ret;
  }

  rightHead.numEntries = MIDDLE_INDEX_LEAF + 1;
  rightHead.pblock = leftHead.pblock;
  rightHead.lblock = leafBlockNum;
  rightHead.rblock = oldRightBlock;
  ret = rightBlk.setHeader(&rightHead);
  if (ret != SUCCESS) {
    return ret;
  }

  for (int i = 0; i <= MIDDLE_INDEX_LEAF; ++i) {
    ret = leftBlk.setEntry(&indices[i], i);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  for (int i = MIDDLE_INDEX_LEAF + 1, j = 0; i <= MAX_KEYS_LEAF; ++i, ++j) {
    ret = rightBlk.setEntry(&indices[i], j);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  if (oldRightBlock != -1) {
    RecBuffer oldRightBuf(oldRightBlock);
    HeadInfo oldRightHead;
    ret = oldRightBuf.getHeader(&oldRightHead);
    if (ret != SUCCESS) {
      return ret;
    }
    oldRightHead.lblock = rightBlkNum;
    ret = oldRightBuf.setHeader(&oldRightHead);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  return rightBlkNum;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
  IndInternal rightBlk;
  IndInternal leftBlk(intBlockNum);

  int rightBlkNum = rightBlk.getBlockNum();
  if (rightBlkNum == E_DISKFULL) {
    return E_DISKFULL;
  }

  HeadInfo leftHead;
  HeadInfo rightHead;
  int ret = leftBlk.getHeader(&leftHead);
  if (ret != SUCCESS) {
    return ret;
  }
  ret = rightBlk.getHeader(&rightHead);
  if (ret != SUCCESS) {
    return ret;
  }

  leftHead.numEntries = MIDDLE_INDEX_INTERNAL;
  ret = leftBlk.setHeader(&leftHead);
  if (ret != SUCCESS) {
    return ret;
  }

  rightHead.numEntries = MIDDLE_INDEX_INTERNAL;
  rightHead.pblock = leftHead.pblock;
  ret = rightBlk.setHeader(&rightHead);
  if (ret != SUCCESS) {
    return ret;
  }

  for (int i = 0; i < MIDDLE_INDEX_INTERNAL; ++i) {
    ret = leftBlk.setEntry(&internalEntries[i], i);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  for (int i = MIDDLE_INDEX_INTERNAL + 1, j = 0; i <= MAX_KEYS_INTERNAL; ++i, ++j) {
    ret = rightBlk.setEntry(&internalEntries[i], j);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  for (int i = MIDDLE_INDEX_INTERNAL + 1; i <= MAX_KEYS_INTERNAL; ++i) {
    ret = updateParentBlock(internalEntries[i].lChild, rightBlkNum);
    if (ret != SUCCESS) {
      return ret;
    }
    ret = updateParentBlock(internalEntries[i].rChild, rightBlkNum);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  IndInternal intBlk(intBlockNum);
  HeadInfo blockHeader;
  ret = intBlk.getHeader(&blockHeader);
  if (ret != SUCCESS) {
    return ret;
  }

  InternalEntry internalEntries[MAX_KEYS_INTERNAL + 1];
  bool inserted = false;
  int insertPos = 0;

  for (int i = 0; i < blockHeader.numEntries; ++i) {
    InternalEntry curr{};
    ret = intBlk.getEntry(&curr, i);
    if (ret != SUCCESS) {
      return ret;
    }

    if (!inserted && compareAttrs(intEntry.attrVal, curr.attrVal, attrCatEntry.attrType) < 0) {
      internalEntries[insertPos++] = intEntry;
      inserted = true;
      curr.lChild = intEntry.rChild;
    }

    internalEntries[insertPos++] = curr;
  }

  if (!inserted) {
    internalEntries[insertPos++] = intEntry;
  }

  if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
    blockHeader.numEntries++;
    ret = intBlk.setHeader(&blockHeader);
    if (ret != SUCCESS) {
      return ret;
    }

    for (int i = 0; i < blockHeader.numEntries; ++i) {
      ret = intBlk.setEntry(&internalEntries[i], i);
      if (ret != SUCCESS) {
        return ret;
      }
    }
    return SUCCESS;
  }

  int newRightBlk = splitInternal(intBlockNum, internalEntries);
  if (newRightBlk == E_DISKFULL) {
    return E_DISKFULL;
  }
  if (newRightBlk < 0) {
    return newRightBlk;
  }

  if (blockHeader.pblock != -1) {
    InternalEntry parentEntry{};
    parentEntry.lChild = intBlockNum;
    assignInternalAttr(&parentEntry, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal);
    parentEntry.rChild = newRightBlk;
    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, parentEntry);
    if (ret != SUCCESS) {
      return ret;
    }
  } else {
    ret = createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  return SUCCESS;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  IndLeaf leafBlk(blockNum);
  HeadInfo blockHeader;
  ret = leafBlk.getHeader(&blockHeader);
  if (ret != SUCCESS) {
    return ret;
  }

  Index indices[MAX_KEYS_LEAF + 1];
  bool inserted = false;
  int insertPos = 0;

  for (int i = 0; i < blockHeader.numEntries; ++i) {
    Index curr{};
    ret = leafBlk.getEntry(&curr, i);
    if (ret != SUCCESS) {
      return ret;
    }

    if (!inserted && compareAttrs(indexEntry.attrVal, curr.attrVal, attrCatEntry.attrType) < 0) {
      indices[insertPos++] = indexEntry;
      inserted = true;
    }

    indices[insertPos++] = curr;
  }

  if (!inserted) {
    indices[insertPos++] = indexEntry;
  }

  if (blockHeader.numEntries != MAX_KEYS_LEAF) {
    blockHeader.numEntries++;
    ret = leafBlk.setHeader(&blockHeader);
    if (ret != SUCCESS) {
      return ret;
    }

    for (int i = 0; i < blockHeader.numEntries; ++i) {
      ret = leafBlk.setEntry(&indices[i], i);
      if (ret != SUCCESS) {
        return ret;
      }
    }
    return SUCCESS;
  }

  int newRightBlk = splitLeaf(blockNum, indices);
  if (newRightBlk == E_DISKFULL) {
    return E_DISKFULL;
  }
  if (newRightBlk < 0) {
    return newRightBlk;
  }

  if (blockHeader.pblock != -1) {
    InternalEntry parentEntry{};
    parentEntry.lChild = blockNum;
    assignInternalAttr(&parentEntry, indices[MIDDLE_INDEX_LEAF].attrVal);
    parentEntry.rChild = newRightBlk;
    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, parentEntry);
    if (ret != SUCCESS) {
      return ret;
    }
  } else {
    ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
    if (ret != SUCCESS) {
      return ret;
    }
  }

  return SUCCESS;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, RecId recordId) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  int rootBlock = attrCatEntry.rootBlock;
  if (rootBlock == -1) {
    return E_NOINDEX;
  }

  int leafBlkNum = findLeafToInsert(rootBlock, attrVal, attrCatEntry.attrType);
  Index indexEntry{};
  assignIndexAttr(&indexEntry, attrVal);
  indexEntry.block = recordId.block;
  indexEntry.slot = recordId.slot;

  ret = insertIntoLeaf(relId, attrName, leafBlkNum, indexEntry);
  if (ret == E_DISKFULL) {
    int destroyRet = bPlusDestroy(rootBlock);
    if (destroyRet != SUCCESS) {
      return destroyRet;
    }
    attrCatEntry.rootBlock = -1;
    destroyRet = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
    if (destroyRet != SUCCESS) {
      return destroyRet;
    }
    return E_DISKFULL;
  }

  return ret;
}

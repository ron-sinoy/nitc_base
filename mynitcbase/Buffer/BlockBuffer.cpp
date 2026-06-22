#include "BlockBuffer.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
  this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blockType) {
  int mappedType;
  if (blockType == 'R') {
    mappedType = REC;
  } else if (blockType == 'I') {
    mappedType = IND_INTERNAL;
  } else if (blockType == 'L') {
    mappedType = IND_LEAF;
  } else {
    mappedType = blockType;
  }

  this->blockNum = getFreeBlock(mappedType);
}

RecBuffer::RecBuffer() : BlockBuffer('R') {}
RecBuffer::RecBuffer(int blockNum) : BlockBuffer(blockNum) {}
IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum) {}
IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType) {}
IndInternal::IndInternal() : IndBuffer('I') {}
IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum) {}
IndLeaf::IndLeaf() : IndBuffer('L') {}
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum) {}


// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;   
  }

  memcpy(head, bufferPtr, HEADER_SIZE);

  return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  memcpy(bufferPtr, head, HEADER_SIZE);
  return StaticBuffer::setDirtyBit(this->blockNum);
}

int BlockBuffer::getBlockNum() {
  return this->blockNum;
}

int BlockBuffer::getFreeBlock(int blockType) {
  for (int blockNum = 0; blockNum < DISK_BLOCKS; ++blockNum) {
    if (StaticBuffer::getStaticBlockType(blockNum) != UNUSED_BLK) {
      continue;
    }

    int bufferNum = StaticBuffer::getFreeBuffer(blockNum);
    if (bufferNum < 0) {
      return bufferNum;
    }

    memset(StaticBuffer::blocks[bufferNum], 0, BLOCK_SIZE);

    HeadInfo head;
    head.blockType = blockType;
    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;
    memset(head.reserved, 0, sizeof(head.reserved));
    memcpy(StaticBuffer::blocks[bufferNum], &head, HEADER_SIZE);

    StaticBuffer::metainfo[bufferNum].dirty = true;
    StaticBuffer::blockAllocMap[blockNum] = blockType;
    return blockNum;
  }

  return E_DISKFULL;
}

int BlockBuffer::setBlockType(int blockType) {
  if (this->blockNum < 0 || this->blockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  StaticBuffer::blockAllocMap[this->blockNum] = blockType;
  return SUCCESS;
}

void BlockBuffer::releaseBlock() {
  if (this->blockNum == INVALID_BLOCKNUM) {
    return;
  }

  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
  if (bufferNum >= 0) {
    StaticBuffer::metainfo[bufferNum].free = true;
    StaticBuffer::metainfo[bufferNum].dirty = false;
    StaticBuffer::metainfo[bufferNum].blockNum = INVALID_BLOCKNUM;
    StaticBuffer::metainfo[bufferNum].timeStamp = -1;
  }

  StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
  this->blockNum = INVALID_BLOCKNUM;
}

// get records
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  struct HeadInfo head;
  BlockBuffer::getHeader(&head);
  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  int recordSize = attrCount * ATTR_SIZE;
  if (slotNum < 0 || slotNum >= slotCount) {
    return E_OUTOFBOUND;
  }

  unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

  memcpy(rec, slotPointer, recordSize);
 
  return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum)
{
    unsigned char *buffer;
    int ret = loadBlockAndGetBufferPtr(&buffer);

    if(ret != SUCCESS) return ret;

    struct HeadInfo head;
    BlockBuffer::getHeader(&head);

    int attrCnt = head.numAttrs;
    int slotCnt = head.numSlots;

    if(slotNum<0 || slotNum>= slotCnt)
        return E_OUTOFBOUND;
    
    int recordSize = attrCnt * ATTR_SIZE;
    unsigned char *slotPointer = buffer + HEADER_SIZE + slotCnt + slotNum * recordSize;
    memcpy(slotPointer,rec,recordSize);

    int retn = StaticBuffer::setDirtyBit(this->blockNum);

    if(retn != SUCCESS) 
        std::cout <<"Error in setDirty function"<<std::endl;
        
    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum == E_BLOCKNOTINBUFFER) {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
    if (bufferNum == E_OUTOFBOUND) {
      return E_OUTOFBOUND;
    }
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // always update timestamps
  for (int i = 0; i < BUFFER_CAPACITY; i++) {
    if (i == bufferNum)
      StaticBuffer::metainfo[i].timeStamp = 0;
    else
      StaticBuffer::metainfo[i].timeStamp++;
  }

  *buffPtr = StaticBuffer::blocks[bufferNum];
  return SUCCESS;
}
int RecBuffer::getSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  struct HeadInfo head;
  BlockBuffer::getHeader(&head);
  int slotCount = head.numSlots;

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  memcpy(slotMap, slotMapInBuffer, slotCount);
  return SUCCESS;
}
int RecBuffer::setSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  HeadInfo head;
  ret = BlockBuffer::getHeader(&head);
  if (ret != SUCCESS) {
    return ret;
  }

  memcpy(bufferPtr + HEADER_SIZE, slotMap, head.numSlots);
  return StaticBuffer::setDirtyBit(this->blockNum);
}

int IndInternal::getEntry(void *ptr, int indexNum) {
  if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + indexNum * INTERNAL_ENTRY_SIZE;
  InternalEntry *internalEntry = static_cast<InternalEntry *>(ptr);
  memcpy(&internalEntry->lChild, entryPtr, sizeof(int32_t));
  memcpy(&internalEntry->attrVal, entryPtr + sizeof(int32_t), sizeof(Attribute));
  memcpy(&internalEntry->rChild, entryPtr + sizeof(int32_t) + sizeof(Attribute), sizeof(int32_t));
  return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum) {
  if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  InternalEntry *internalEntry = static_cast<InternalEntry *>(ptr);
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + indexNum * INTERNAL_ENTRY_SIZE;
  memcpy(entryPtr, &internalEntry->lChild, sizeof(int32_t));
  memcpy(entryPtr + sizeof(int32_t), &internalEntry->attrVal, sizeof(Attribute));
  memcpy(entryPtr + sizeof(int32_t) + sizeof(Attribute), &internalEntry->rChild, sizeof(int32_t));
  return StaticBuffer::setDirtyBit(this->blockNum);
}

int IndLeaf::getEntry(void *ptr, int indexNum) {
  if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF) {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  memcpy(ptr, bufferPtr + HEADER_SIZE + indexNum * LEAF_ENTRY_SIZE, LEAF_ENTRY_SIZE);
  return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum) {
  if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF) {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  memcpy(bufferPtr + HEADER_SIZE + indexNum * LEAF_ENTRY_SIZE, ptr, LEAF_ENTRY_SIZE);
  return StaticBuffer::setDirtyBit(this->blockNum);
}
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType)
{
    double diff;
    if(attrType == 1)
        diff = strcmp(attr1.sVal, attr2.sVal);
    else    
        diff = attr1.nVal - attr2.nVal;
    
    if (diff > 0) return 1;
    if(diff < 0) return -1;
    if (diff == 0) return 0;
    return 0;
}

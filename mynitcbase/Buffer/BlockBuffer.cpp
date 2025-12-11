#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

//1
BlockBuffer::BlockBuffer(int blockNum) {
  this->blockNum = blockNum;
}
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}


// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;   
  }

  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, this->blockNum);

  memcpy(&head->numSlots, buffer + 24, 4);
  memcpy(&head->numEntries, buffer + 16, 4);
  memcpy(&head->numAttrs,buffer + 20, 4);
  memcpy(&head->rblock, buffer + 12, 4);
  memcpy(&head->lblock, buffer + 8, 4);

  return SUCCESS;
}

 
//get records 
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

  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, this->blockNum);

  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = buffer + (HEADER_SIZE+slotCount+(recordSize*slotNum));

  memcpy(rec, slotPointer, recordSize);
 
  return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
  // check whether the block is already present in the buffer using StaticBuffer.getBufferNum()
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum == E_BLOCKNOTINBUFFER) {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum == E_OUTOFBOUND) {
      return E_OUTOFBOUND;
    }

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}

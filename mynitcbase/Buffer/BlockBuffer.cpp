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


#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {

  for (int bufferIndex = 0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].timeStamp = -1;
    metainfo[bufferIndex].blockNum = -1;
  }
}

StaticBuffer::~StaticBuffer() {
   for(int i = 0; i<BUFFER_CAPACITY; i++){
    if(!metainfo[i].free && metainfo[i].dirty){
      Disk::writeBlock(blocks[i],metainfo[i].blockNum); 
    }
   }
    

}

int StaticBuffer::getFreeBuffer(int blockNum) {
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }
  for(int i = 0; i<BUFFER_CAPACITY; i++){
    if(!metainfo[i].free){
      metainfo[i].timeStamp++;
    }
  }
  int bufferNum = -1;

  for (int bufferIndex = 0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
    if (metainfo[bufferIndex].free == true){
    bufferNum = bufferIndex;
    break;
    }
  }
  int timeStampMax = -1;
  if(bufferNum == -1){
    for(int i = 0; i<BUFFER_CAPACITY; i++){
      if(metainfo[i].timeStamp>timeStampMax){
        timeStampMax = metainfo[i].timeStamp;
        bufferNum = i;
      }
    }
  }
  if(metainfo[bufferNum].dirty){
    Disk::writeBlock(blocks[bufferNum],metainfo[bufferNum].blockNum); 
  }
  metainfo[bufferNum].free = false;
  metainfo[bufferNum].blockNum = blockNum;
  metainfo[bufferNum].timeStamp = 0;
  return bufferNum;
}


int StaticBuffer::getBufferNum(int blockNum) {
  // Check if blockNum is valid (between zero and DISK_BLOCKS)
  // and return E_OUTOFBOUND if not valid.

    if (blockNum < 0 || blockNum > DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  // find and return the bufferIndex which corresponds to blockNum (check metainfo)
  for (int bufferIndex = 0; bufferIndex<BUFFER_CAPACITY; bufferIndex++) {
    if (blockNum ==   metainfo[bufferIndex].blockNum){
        return bufferIndex;
    }
  }
  // if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum){
    int bufferNum = StaticBuffer::getBufferNum(blockNum);
    if(bufferNum == E_BLOCKNOTINBUFFER) return E_BLOCKNOTINBUFFER; 
    if(bufferNum == E_OUTOFBOUND) return E_OUTOFBOUND;
    
    metainfo[bufferNum].dirty = true;
    return SUCCESS;
}
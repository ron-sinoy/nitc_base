#include "BlockAccess.h"

#include <cstring>
RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);
    
    int block; //use -1 if error arises
    int slot;
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId,&relCatBuf);
        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else
    {
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }


    while (block != -1)
    {
        RecBuffer recBuffer(block);
        HeadInfo head;
        
        recBuffer.getHeader(&head);
        
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        if(slot >= head.numSlots)
        {
            block = head.rblock;
            slot = 0;
            continue;  
        }

        if(slotMap[slot] == SLOT_UNOCCUPIED){
            slot++;
            continue;
        }

        AttrCatEntry attrCatBuf;
        AttrCacheTable::getAttrCatEntry(relId,attrName, &attrCatBuf);

        Attribute rec[head.numAttrs];
        recBuffer.getRecord(rec, slot);
        int attrOffset = attrCatBuf.offset;
        int cmpVal = compareAttrs(rec[attrOffset], attrVal, attrCatBuf.attrType); 
        

        if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
           RecId searchInd;
           searchInd.block = block;
           searchInd.slot = slot;

           RelCacheTable::setSearchIndex(relId,&searchInd);

           return RecId{block, slot};
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}


int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{
    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newRelationName;    // set newRelationName with newName
    strcpy(newRelationName.sVal,newName);

    // search the relation catalog for an entry with "RelName" = newRelationName

    // If relation with name newName already exists (result of linearSearch
    //                                               is not {-1, -1})
    //    return E_RELEXIST;
    char relCatAttrRelName[ATTR_SIZE];
    strcpy(relCatAttrRelName,RELCAT_ATTR_RELNAME);
    RecId existRelRecId = BlockAccess::linearSearch(RELCAT_RELID,relCatAttrRelName,newRelationName, EQ);
    if(existRelRecId.block != -1 && existRelRecId.slot != -1)
        return E_RELEXIST;

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;    // set oldRelationName with oldName
    strcpy(oldRelationName.sVal,oldName);

    // search the relation catalog for an entry with "RelName" = oldRelationName

    // If relation with name oldName does not exist (result of linearSearch is {-1, -1})
    //    return E_RELNOTEXIST;
     RecId oldRelRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, oldRelationName, EQ);
    if(oldRelRecId.block == -1 || oldRelRecId.slot == -1)
        return E_RELNOTEXIST;

    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
    /* update the relation name attribute in the record with newName.
       (use RELCAT_REL_NAME_INDEX) */
    // set back the record value using RecBuffer.setRecord
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord,oldRelRecId.slot);

    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
    relCatBlock.setRecord(relCatRecord, oldRelRecId.slot);

    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
       RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    //for i = 0 to numberOfAttributes :
    //    linearSearch on the attribute catalog for relName = oldRelationName
    //    get the record using RecBuffer.getRecord
    //
    //    update the relName field in the record to newName
    //    set back the record using RecBuffer.setRecord

    for (int i=0; i<relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; i++)
    {
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName,oldRelationName, EQ);
        RecBuffer attrCatBlock(attrCatRecId.block);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
        attrCatBlock.setRecord(attrCatRecord, attrCatRecId.slot);
    }

    return SUCCESS;
}


int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {


    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute relNameAttr;    // set relNameAttr to relName
    strcpy(relNameAttr.sVal,relName);

    char relCatAttrRelName[ATTR_SIZE];
    strcpy(relCatAttrRelName,RELCAT_ATTR_RELNAME);

    RecId existRecId = BlockAccess::linearSearch(RELCAT_RELID,relCatAttrRelName, relNameAttr,EQ);
    if(existRecId.block == -1 && existRecId.slot == -1) 
        return E_RELNOTEXIST;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    Attribute temp;
    strcpy(temp.sVal, relName);

    while (true) {
        RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID,relCatAttrRelName,temp,EQ);

        if(attrRecId.block == -1 && attrRecId.slot == -1)
            break;
        
        else
        {

          RecBuffer attrCatEntryBlock(attrRecId.block);
          attrCatEntryBlock.getRecord(attrCatEntryRecord, attrRecId.slot);


        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,oldName) == 0)
            attrToRenameRecId = attrRecId;


        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
            return E_ATTREXIST;
        }
        
    }

    if(attrToRenameRecId.block == -1 || attrToRenameRecId.slot == -1)
        return E_ATTRNOTEXIST;

    RecBuffer attrCatBlock(attrToRenameRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrToRenameRecId.slot);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    attrCatBlock.setRecord(attrCatRecord, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, union Attribute *record) {
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    int blockNum = relCatEntry.firstBlk;
    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;
    int prevBlockNum = -1;
    RecId recId{-1, -1};

    while (blockNum != -1) {
        RecBuffer recBuffer(blockNum);
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

        for (int slotNum = 0; slotNum < head.numSlots; ++slotNum) {
            if (slotMap[slotNum] == SLOT_UNOCCUPIED) {
                recId.block = blockNum;
                recId.slot = slotNum;
                break;
            }
        }

        if (recId.block != -1) {
            break;
        }

        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    if (recId.block == -1) {
        if (relId == RELCAT_RELID) {
            return E_MAXRELATIONS;
        }

        RecBuffer newRecBlock;
        int newBlockNum = newRecBlock.getBlockNum();
        if (newBlockNum == E_DISKFULL) {
            return E_DISKFULL;
        }

        recId.block = newBlockNum;
        recId.slot = 0;

        HeadInfo head;
        head.blockType = REC;
        head.pblock = -1;
        head.lblock = (prevBlockNum == -1) ? -1 : prevBlockNum;
        head.rblock = -1;
        head.numEntries = 0;
        head.numAttrs = numOfAttributes;
        head.numSlots = numOfSlots;
        memset(head.reserved, 0, sizeof(head.reserved));
        ret = newRecBlock.setHeader(&head);
        if (ret != SUCCESS) {
            return ret;
        }

        unsigned char slotMap[numOfSlots];
        for (int i = 0; i < numOfSlots; ++i) {
            slotMap[i] = SLOT_UNOCCUPIED;
        }
        ret = newRecBlock.setSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        if (prevBlockNum != -1) {
            RecBuffer prevBlock(prevBlockNum);
            HeadInfo prevHead;
            ret = prevBlock.getHeader(&prevHead);
            if (ret != SUCCESS) {
                return ret;
            }

            prevHead.rblock = newBlockNum;
            ret = prevBlock.setHeader(&prevHead);
            if (ret != SUCCESS) {
                return ret;
            }
        } else {
            relCatEntry.firstBlk = newBlockNum;
        }

        relCatEntry.lastBlk = newBlockNum;

        ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    RecBuffer targetBlock(recId.block);
    HeadInfo head;
    ret = targetBlock.getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    ret = targetBlock.setRecord(record, recId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    unsigned char slotMap[head.numSlots];
    ret = targetBlock.getSlotMap(slotMap);
    if (ret != SUCCESS) {
        return ret;
    }
    slotMap[recId.slot] = SLOT_OCCUPIED;
    ret = targetBlock.setSlotMap(slotMap);
    if (ret != SUCCESS) {
        return ret;
    }

    head.numEntries++;
    ret = targetBlock.setHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    relCatEntry.numRecs++;
    ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    int status = SUCCESS;
    for (int attrOffset = 0; attrOffset < numOfAttributes; ++attrOffset) {
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(relId, attrOffset, &attrCatEntry);
        if (ret != SUCCESS) {
            return ret;
        }

        if (attrCatEntry.rootBlock == -1) {
            continue;
        }

        ret = BPlusTree::bPlusInsert(relId, attrCatEntry.attrName, record[attrOffset], recId);
        if (ret == E_DISKFULL) {
            status = E_INDEX_BLOCKS_RELEASED;
        } else if (ret != SUCCESS) {
            return ret;
        }
    }

    return status;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    RecId recId;
    if (attrCatEntry.rootBlock == -1) {
        recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);
    } else {
        recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
    }

    if (recId.block == -1 && recId.slot == -1) {
        return E_NOTFOUND;
    }

    RecBuffer recBuffer(recId.block);
    return recBuffer.getRecord(record, recId.slot);
}

int BlockAccess::project(int relId, Attribute *record) {
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    RecId searchIndex;
    ret = RelCacheTable::getSearchIndex(relId, &searchIndex);
    if (ret != SUCCESS) {
        return ret;
    }

    int block = searchIndex.block;
    int slot = searchIndex.slot + 1;
    if (block == -1 && slot == 0) {
        block = relCatEntry.firstBlk;
        slot = 0;
    }

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

        while (slot < head.numSlots) {
            if (slotMap[slot] == SLOT_UNOCCUPIED) {
                ++slot;
                continue;
            }

            ret = recBuffer.getRecord(record, slot);
            if (ret != SUCCESS) {
                return ret;
            }

            RecId nextIndex{block, slot};
            ret = RelCacheTable::setSearchIndex(relId, &nextIndex);
            if (ret != SUCCESS) {
                return ret;
            }

            return SUCCESS;
        }

        block = head.rblock;
        slot = 0;
    }

    return E_NOTFOUND;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    if (relCatRecId.block == -1 && relCatRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    RecBuffer relCatBlock(relCatRecId.block);
    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    int ret = relCatBlock.getRecord(relCatEntryRecord, relCatRecId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int blockNum = firstBlock;

    while (blockNum != -1) {
        RecBuffer recBlock(blockNum);
        HeadInfo header;
        ret = recBlock.getHeader(&header);
        if (ret != SUCCESS) {
            return ret;
        }

        int nextBlock = header.rblock;
        recBlock.releaseBlock();
        blockNum = nextBlock;
    }

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numberOfAttributesDeleted = 0;

    while (true) {
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1) {
            break;
        }

        numberOfAttributesDeleted++;

        RecBuffer attrCatBlock(attrCatRecId.block);
        HeadInfo header;
        ret = attrCatBlock.getHeader(&header);
        if (ret != SUCCESS) {
            return ret;
        }

        Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
        ret = attrCatBlock.getRecord(attrCatEntryRecord, attrCatRecId.slot);
        if (ret != SUCCESS) {
            return ret;
        }

        int rootBlock = attrCatEntryRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        unsigned char slotMap[header.numSlots];
        ret = attrCatBlock.getSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        ret = attrCatBlock.setSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        header.numEntries--;
        ret = attrCatBlock.setHeader(&header);
        if (ret != SUCCESS) {
            return ret;
        }

        if (rootBlock != -1) {
            ret = BPlusTree::bPlusDestroy(rootBlock);
            if (ret != SUCCESS) {
                return ret;
            }
        }

        if (header.numEntries == 0) {
            int leftBlock = header.lblock;
            int rightBlock = header.rblock;

            if (leftBlock != -1) {
                RecBuffer leftRecBlock(leftBlock);
                HeadInfo leftHeader;
                ret = leftRecBlock.getHeader(&leftHeader);
                if (ret != SUCCESS) {
                    return ret;
                }
                leftHeader.rblock = rightBlock;
                ret = leftRecBlock.setHeader(&leftHeader);
                if (ret != SUCCESS) {
                    return ret;
                }
            }

            if (rightBlock != -1) {
                RecBuffer rightRecBlock(rightBlock);
                HeadInfo rightHeader;
                ret = rightRecBlock.getHeader(&rightHeader);
                if (ret != SUCCESS) {
                    return ret;
                }
                rightHeader.lblock = leftBlock;
                ret = rightRecBlock.setHeader(&rightHeader);
                if (ret != SUCCESS) {
                    return ret;
                }

                RecBuffer leftRecBlock(leftBlock);
                HeadInfo leftHeader;
                ret = leftRecBlock.getHeader(&leftHeader);
                if (ret != SUCCESS) {
                    return ret;
                }
                RecId searchIndex{leftBlock, leftHeader.numSlots - 1};
                ret = RelCacheTable::setSearchIndex(ATTRCAT_RELID, &searchIndex);
                if (ret != SUCCESS) {
                    return ret;
                }
            } else {
                RelCatEntry attrCatRelEntry;
                ret = RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatRelEntry);
                if (ret != SUCCESS) {
                    return ret;
                }
                attrCatRelEntry.lastBlk = leftBlock;
                ret = RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatRelEntry);
                if (ret != SUCCESS) {
                    return ret;
                }
            }

            attrCatBlock.releaseBlock();
            if (rightBlock == -1) {
                break;
            }
        }
    }

    HeadInfo relCatHeader;
    ret = relCatBlock.getHeader(&relCatHeader);
    if (ret != SUCCESS) {
        return ret;
    }
    relCatHeader.numEntries--;
    ret = relCatBlock.setHeader(&relCatHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    unsigned char relCatSlotMap[relCatHeader.numSlots];
    ret = relCatBlock.getSlotMap(relCatSlotMap);
    if (ret != SUCCESS) {
        return ret;
    }
    relCatSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
    ret = relCatBlock.setSlotMap(relCatSlotMap);
    if (ret != SUCCESS) {
        return ret;
    }

    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }
    relCatEntry.numRecs--;
    ret = RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    RelCatEntry attrCatEntry;
    ret = RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }
    attrCatEntry.numRecs -= numberOfAttributesDeleted;
    ret = RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}

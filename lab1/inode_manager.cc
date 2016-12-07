#include "inode_manager.h"
#include "assert.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
  //assert(buf!=NULL);
  //assert(id>=0);
  //assert(id<=BLOCK_SIZE);
  if(buf==NULL||id<0||id>BLOCK_NUM)
    return;
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
  //assert(buf!=NULL);
  //assert(id>=0);
  //assert(id<=BLOCK_SIZE);
  if(buf==NULL||id<0||id>BLOCK_NUM)
    return;
  //printf("write_block:%d %s\n",id,buf);
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
   uint32_t ninodes = sb.ninodes;
   uint32_t nblocks = sb.nblocks;
   blockid_t fileBlockId_first = IBLOCK(ninodes, nblocks);
   //printf("fileblock first = %d\n",fileBlockId_first);

   //try to find free block
   for(blockid_t blockId = fileBlockId_first;blockId<BLOCK_NUM;blockId++){
     std::map <uint32_t, int> ::iterator it;
     it=using_blocks.find(blockId);
     //if find the free block
     if(it==using_blocks.end()){
       //printf("get free block = %d\n",blockId);
       //edit using_blocks
       using_blocks.insert(std::pair<uint32_t, int>(blockId,1));
       //get the block of bitmap
       blockid_t bmId = BBLOCK(blockId);
       uint32_t bmCharOffset = (blockId%BPB)/8;
       int bmBitOffset = (blockId%BPB)%8;
       char *blockStr = (char *)malloc(BLOCK_SIZE);
       read_block(bmId,blockStr);
       char ch = blockStr[bmCharOffset];
       char hch = (char)(1<<(7-bmBitOffset));
       blockStr[bmCharOffset] = ch|hch;

       write_block(bmId,blockStr);
       delete[] blockStr;
       return blockId;

     }
   }

  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /*
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
   uint32_t ninodes = sb.ninodes;
   uint32_t nblocks = sb.nblocks;

   std::map <uint32_t, int> ::iterator it;
   it=using_blocks.find(id);
   //if find the free block
   if(it!=using_blocks.end()){
     using_blocks.erase(id);
       //get the block of bitmap
       blockid_t bmId = BBLOCK(id);
       uint32_t bmCharOffset = (id%BPB)/8;
       int bmBitOffset = (id%BPB)%8;
       char *blockStr = (char *)malloc(BLOCK_SIZE);
       read_block(bmId,blockStr);
       char ch = blockStr[bmCharOffset];
       char hch = (char)(~(1<<(7-bmBitOffset)));
       blockStr[bmCharOffset] = ch&hch;
       write_block(bmId,blockStr);
       delete[] blockStr;
   }
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /*
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().

   * if you get some heap memory, do not forget to free it.
   */

   for(uint32_t inum = 1;inum<INODE_NUM;inum++){
     struct inode *inode_p = get_inode(inum);
     if(inode_p==NULL){
        inode_p = (struct inode *)malloc(sizeof(struct inode));
        inode_p->type = type;
        put_inode(inum,inode_p);
        delete inode_p;
        return inum;
     }
   }
  return 1;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /*
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
   struct inode *ino = get_inode(inum);
   if(ino==NULL) return;
   struct inode *blank_ino = (struct inode*)malloc(sizeof(struct inode));
   blank_ino->type = 0;
   put_inode(inum,blank_ino);
   delete blank_ino;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode*
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));

  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))


/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
   struct inode *ino = get_inode(inum);
   if(ino==NULL) return;
   //file size
   if(size == NULL)
     size = (int *)malloc(sizeof(int));
   *size = ino->size;
   //printf("read 1nd block %d from inum %d\n",ino->blocks[0],inum);

   int fileSize = ino->size;
   int blockSize = ((ino->size-1)/BLOCK_SIZE+1)*BLOCK_SIZE;
   //use as iterator
   blockid_t blockNum = 0;
   //the data has been load into buf_out
   unsigned int loadSize = 0;

   //malloc buf_out
   //*buf_out = (char *)malloc(fileSize);
   *buf_out = (char *)malloc(blockSize);


   //ndirect blocks
   while(blockNum<NDIRECT&&loadSize<fileSize){
     char* blockBuf = (char *)malloc(BLOCK_SIZE);
     //find the position now
     char* outPos = *buf_out+(blockNum)*BLOCK_SIZE;
     //bm->read_block(ino->blocks[blockNum],blockBuf);
     bm->read_block(ino->blocks[blockNum],outPos);
     int readSize = MIN(BLOCK_SIZE,fileSize-loadSize);
     loadSize+=readSize;
     //store into out_buf
     //memcpy(outPos, blockBuf, readSize);
     blockNum++;
     delete[] blockBuf;
   }
   //printf("read %s from inum %d\n",*buf_out,inum);
   if(loadSize==fileSize)
     return;

   blockid_t *IdirectBlockBuf = (blockid_t *)malloc(BLOCK_SIZE);
   bm->read_block(ino->blocks[blockNum],(char *)IdirectBlockBuf);
   blockNum = 0;
   while(blockNum<BLOCK_SIZE/sizeof(uint32_t)&&loadSize<fileSize){
     char* blockBuf = (char *)malloc(BLOCK_SIZE);
     //find the position now
     char* outPos = *buf_out+(blockNum+NDIRECT)*BLOCK_SIZE;
     //bm->read_block(IdirectBlockBuf[blockNum],blockBuf);
     bm->read_block(IdirectBlockBuf[blockNum],outPos);
     int readSize = MIN(BLOCK_SIZE,fileSize-loadSize);
     loadSize+=readSize;
     //store into out_buf
     //memcpy(outPos, blockBuf, readSize);
     blockNum++;
     delete[] blockBuf;
   }
   //printf("read %s from inum %d\n",*buf_out,inum);

}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */

   //printf("write %s into inum %d\n",buf,inum);

   struct inode *ino = get_inode(inum);

   if(ino==NULL||ino->type==0||buf==NULL||size<0){
     printf("inode error\n");
     return;
   }
   //printf("write 1nd block %d from inum %d\n",ino->blocks[0],inum);

   unsigned int oldSize = ino->size;
   unsigned int oldBlockNum = oldSize==0?0:((oldSize-1)/BLOCK_SIZE+1);
   unsigned int newBlockNum = size==0?0:((size-1)/BLOCK_SIZE+1);
   unsigned int oldMaxSize = oldBlockNum*BLOCK_SIZE;

   //printf("oldsize = %d;oldblocknum = %d\n",oldSize,oldBlockNum);

   unsigned int blockNum = 0;

   //if smaller
   if(oldMaxSize>=size) {
     //printf("%s\n", "oldMaxSize>=size");
     unsigned int doneSize = 0;
     blockNum = 0;

     //write NDIRECT
     while(blockNum<NDIRECT&&doneSize < size){
       //char* blockBuf = (char *)malloc(BLOCK_SIZE);
       const char* inPos = buf+(blockNum)*BLOCK_SIZE;
       int writeSize = MIN(BLOCK_SIZE,size-doneSize);
       doneSize+=writeSize;
       //memcpy(blockBuf, inPos, writeSize);
       //bm->write_block(ino->blocks[blockNum],blockBuf);
       bm->write_block(ino->blocks[blockNum],inPos);
       blockNum++;
       //delete[] blockBuf;
     }
     //if new<=NDIRECT then free the blocks
     if(doneSize == size){
       //free blocks NDIRECT
       while(blockNum<NDIRECT){
         bm->free_block(ino->blocks[blockNum]);
         blockNum++;
       }
       if(oldBlockNum>=NDIRECT){
         //free blocks NINDIRECT
         blockid_t *IdirectBlockBuf = (blockid_t *)malloc(BLOCK_SIZE);
         bm->read_block(ino->blocks[NDIRECT],(char *)IdirectBlockBuf);
         blockNum = 0;
         while(blockNum<NINDIRECT){
           bm->free_block(IdirectBlockBuf[blockNum]);
           blockNum++;
         }
         delete[] IdirectBlockBuf;
       }
       ino->size = size;
       //newIno->mtime = mtime;
       put_inode(inum, ino);
       return;
     }

     //if new>NDIRECT
     else{
       blockid_t *IdirectBlockBuf = (blockid_t *)malloc(BLOCK_SIZE);
       bm->read_block(ino->blocks[NDIRECT],(char *)IdirectBlockBuf);
       blockNum = 0;
       while(blockNum<NINDIRECT&&doneSize<size){
         //char* blockBuf = (char *)malloc(BLOCK_SIZE);
         const char* inPos = buf+(blockNum+NDIRECT)*BLOCK_SIZE;
         int writeSize = MIN(BLOCK_SIZE,size-doneSize);
         doneSize+=writeSize;
         //memcpy(blockBuf, inPos, writeSize);
         //bm->write_block(ino->blocks[blockNum],blockBuf);
         bm->write_block(ino->blocks[blockNum],inPos);
         blockNum++;
         //delete[] blockBuf;
       }
       while(blockNum<NINDIRECT){
         bm->free_block(IdirectBlockBuf[blockNum]);
         blockNum++;
       }
       delete[] IdirectBlockBuf;
       ino->size = size;
       //newIno->mtime = mtime;
       put_inode(inum, ino);
       return;
     }
   }else{
   //if larger
   //printf("%s\n", "oldMaxSize<size");
     //new file <= NDIRECT
     if(newBlockNum<=NDIRECT){

       //printf("WRITEFILE:new>old&&new<NDIRECT\n");
       unsigned int doneSize = 0;
       blockNum = 0;

       //write NDIRECT
       while(blockNum<oldBlockNum && doneSize < size){
         //char* blockBuf = (char *)malloc(BLOCK_SIZE);
         const char* inPos = buf+(blockNum)*BLOCK_SIZE;
         int writeSize = MIN(BLOCK_SIZE,size-doneSize);
         doneSize+=writeSize;
         //memcpy(blockBuf, inPos, writeSize);
         //bm->write_block(ino->blocks[blockNum],blockBuf);
         bm->write_block(ino->blocks[blockNum],inPos);
         blockNum++;
         //delete[] blockBuf;
       }

       while(blockNum<NDIRECT && doneSize < size){
         const char* inPos = buf+(blockNum)*BLOCK_SIZE;
         int writeSize = MIN(BLOCK_SIZE,size-doneSize);
         doneSize+=writeSize;
         blockid_t newBlockId = bm->alloc_block();
         //printf("WRITEFILE:newBlockId %d file=%s",newBlockId,inPos);
         bm->write_block(newBlockId,inPos);
         ino->blocks[blockNum]=newBlockId;
         blockNum++;
       }
       ino->size = size;
       //newIno->mtime = mtime;
       put_inode(inum, ino);
       return;
     }else{
       //new file > NDIRECT
       unsigned int doneSize = 0;
       blockNum = 0;

       //write NDIRECT && <oldsize
       while(blockNum<NDIRECT&&doneSize < oldMaxSize){
         //char* blockBuf = (char *)malloc(BLOCK_SIZE);
         const char* inPos = buf+(blockNum)*BLOCK_SIZE;

         int writeSize = MIN(BLOCK_SIZE,size-doneSize);
         doneSize+=writeSize;
         //memcpy(blockBuf, inPos, writeSize);
         //bm->write_block(ino->blocks[blockNum],blockBu%sf);
         bm->write_block(ino->blocks[blockNum],inPos);
         blockNum++;
         //delete[] blockBuf;
       }

       //write NDIRECT && >=oldsizeoldBlockNum
       while(blockNum<NDIRECT){
         //printf(">=oldsize&&NDIRECT\n");
         const char* inPos = buf+(blockNum)*BLOCK_SIZE;
         int writeSize = MIN(BLOCK_SIZE,size-doneSize);
         doneSize+=writeSize;
         blockid_t newBlockId = bm->alloc_block();
         bm->write_block(newBlockId,inPos);
         ino->blocks[blockNum]=newBlockId;
         blockNum++;
       }

       //if the NINDIRECT entry is empty, alloc a block for it
       if(oldBlockNum<=NDIRECT){
         //printf("the NINDIRECT entry is empty\n");
         ino->blocks[NDIRECT]=bm->alloc_block();
       }

       blockid_t *IdirectBlockBuf = (blockid_t *)malloc(BLOCK_SIZE);//NINDIRECT
       bm->read_block(ino->blocks[NDIRECT],(char *)IdirectBlockBuf);
       blockNum = 0;

       //write NINDIRECT && <oldsize
       while(blockNum + NDIRECT<oldBlockNum){
           //printf("NINDIRECT && <oldsize\n");
           //char* blockBuf = (char *)malloc(BLOCK_SIZE);
           const char* inPos = buf+(blockNum+NDIRECT)*BLOCK_SIZE;
           int writeSize = MIN(BLOCK_SIZE,size-doneSize);
           doneSize+=writeSize;
           //memcpy(blockBuf, inPos, writeSize);
           //bm->write_block(ino->blocks[blockNum],blockBuf);
           bm->write_block(IdirectBlockBuf[blockNum],inPos);
           blockNum++;
           //delete[] blockBuf;
       }

       //write NINDIRECT && >=oldsize
       while(doneSize<size){
           //printf("NINDIRECT && >=oldsize\n");
           blockid_t newBlockId = bm->alloc_block();
           IdirectBlockBuf[blockNum] = newBlockId;
           //char* blockBuf = (char *)malloc(BLOCK_SIZE);
           const char* inPos = buf+(blockNum+NDIRECT)*BLOCK_SIZE;
           int writeSize = MIN(BLOCK_SIZE,size-doneSize);
           doneSize+=writeSize;
           //memcpy(blockBuf, inPos, writeSize);
           //bm->write_block(ino->blocks[blockNum],blockBuf);
           bm->write_block(newBlockId,inPos);
           blockNum++;
           //delete[] blockBuf;
       }

       bm->write_block(ino->blocks[NDIRECT],(char *)IdirectBlockBuf);
       ino->size = size;
       //newIno->mtime = mtime;
       put_inode(inum, ino);
       return;
     }
   }
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
   struct inode *ino = get_inode(inum);
   if(ino==NULL)
     return;
   a.type=(uint32_t)ino->type;
   a.size=ino->size;
   a.atime=ino->atime;
   a.mtime=ino->mtime;
   a.ctime=ino->ctime;
   return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
   struct inode *ino = get_inode(inum);
   if(inum==NULL)
     return;
   int blockNum = ino->size==0?0:(ino->size-1)/BLOCK_SIZE+1;

   for(int i=0;i<NDIRECT&&i<blockNum;i++){
     bm->free_block(ino->blocks[i]);
   }
   if(blockNum>NDIRECT){
     blockid_t *IdirectBlockBuf = (blockid_t *)malloc(BLOCK_SIZE);//NINDIRECT
     bm->read_block(ino->blocks[NDIRECT],(char *)IdirectBlockBuf);

     for(int i = 0;i<NINDIRECT;i++){
       bm->free_block(IdirectBlockBuf[i]);
     }
     delete []IdirectBlockBuf;
     bm->free_block(ino->blocks[NDIRECT]);
   }
   free_inode(inum);

}

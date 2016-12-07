#include "inode_manager.h"

//#define DEBUG

// First data block
#define DBLOCK (BLOCK_NUM / BPB + INODE_NUM + 4)

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
  if(buf==NULL||id<0||id>=BLOCK_NUM)
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
  if(buf==NULL||id<0||id>=BLOCK_NUM)
    return;

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
   */

  blockid_t blockId;
  char blockStr[BLOCK_SIZE];


  for (blockId = BLOCK_NUM / BPB + INODE_NUM + 4; blockId < BLOCK_NUM; blockId++) {
    uint32_t off = blockId % BPB;
    read_block(BBLOCK(blockId), blockStr);

    // if free
    if ((~(blockStr[off / 8] | (~(1 << (off % 8))))) != 0) {
      blockStr[off / 8] |= 1 << (off % 8);
      write_block(BBLOCK(blockId), blockStr);
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
  
  uint32_t off;
  char buf[BLOCK_SIZE];

  if (id < DBLOCK || id >= BLOCK_NUM) {
    return;
  }

  off = id % BPB;
  read_block(BBLOCK(id), buf);

  //free block in bitmap
  buf[off / 8] &= ~(1 << (off % 8));

  write_block(BBLOCK(id), buf);
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
  
  
  // alloc metadata block
	alloc_block();		// boot block
	alloc_block();		// superblock
	for (uint32_t i = 0; i < BLOCK_NUM/BPB; i++)
		alloc_block();	// bitmap block
	for (uint32_t i = 0; i < INODE_NUM/IPB; i++)
		alloc_block();	// inode_table block
  
  char buf[BLOCK_SIZE];
  bzero(buf, sizeof(buf));
  memcpy(buf, &sb, sizeof(sb));
  write_block(1, buf);

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
   */

  struct inode *ino;
  char inodeBuf[BLOCK_SIZE];

  for (uint32_t inum = 1; inum <= INODE_NUM; inum++) {
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);//dont use inode function here
    ino = (struct inode*) inodeBuf;
    if (ino->type == 0) {
      ino->type = type;
      ino->size = 0;
      ino->ctime = ino->mtime = ino->atime = time(NULL);
      bm->write_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);
      return inum;
    }
  }

  return 0; //failed
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  char inodeBuf[BLOCK_SIZE];
  struct inode *ino;
  if (inum <= 0 || inum >= INODE_NUM) 
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);//dont use inode function here
  ino = (struct inode*) inodeBuf;//change type 
  ino->type = 0;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  // modified, "inum < 0" ==> "inum <= 0"
  if (inum <= 0 || inum >= INODE_NUM) {
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
  
  struct inode *ino;
  char inoStr[BLOCK_SIZE];
  char blockStr[BLOCK_SIZE];
  char *retBuf;



  if (inum <= 0 || inum >= INODE_NUM) {
    return;
  }

//change access time
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inoStr);
  ino = (struct inode*) inoStr;
  ino->atime = time(NULL);
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), inoStr);

//do sth
  if (ino->size == 0)
    return;
  *size = ino->size;
  retBuf = (char *) malloc(ino->size);
  *buf_out = retBuf;


//
  int leftSize = ino->size;
  int readSize = BLOCK_SIZE;
  
// for direct case
  for (int i = 0; i < NDIRECT; i++) {
    if (leftSize <= 0)
      return; 
    readSize = MIN(BLOCK_SIZE, leftSize);
    bm->read_block(ino->blocks[i], blockStr);
    leftSize -= BLOCK_SIZE;
    memcpy(retBuf + i * BLOCK_SIZE, blockStr, readSize);
  }

  blockid_t *indArr;
// for indirect case
  bm->read_block(ino->blocks[NDIRECT], inoStr);
  indArr = (blockid_t *) inoStr;
  for (uint i = 0; i < NINDIRECT; i++) {
    if (leftSize <= 0)
      return;
    readSize = MIN(BLOCK_SIZE, leftSize);
    bm->read_block(indArr[i], blockStr);
    leftSize -= BLOCK_SIZE;
    memcpy(retBuf + (NDIRECT + i) * BLOCK_SIZE, blockStr, readSize);
  }
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  struct inode *ino;
  char inoStr[BLOCK_SIZE];
  char blockStr[BLOCK_SIZE];

  int blockNum;

  if (inum <= 0 || inum >= INODE_NUM||size < 0 || size > MAXFILE * BLOCK_SIZE) {
    return;
  }

//get inode
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inoStr);
  ino = (struct inode*) inoStr;
  
  
  ino->mtime = time(NULL);
  if (size != ino->size)
      ino->ctime = ino->mtime;

//get block num
  int oldBlockNum = (ino->size==0) ? 0 :((int)(ino->size) - 1) / BLOCK_SIZE+1;
  int newBlockNum = (size - 1) / BLOCK_SIZE+1;

  //do direct blocks
  int leftSize = size;
  int writeSize = BLOCK_SIZE;
  for (blockNum = 0; blockNum < NDIRECT; blockNum++) {
    if (leftSize > 0) {
      writeSize = MIN(BLOCK_SIZE, leftSize);
      //write to right posi
      memcpy(blockStr, buf + blockNum * BLOCK_SIZE, writeSize);
      leftSize -= BLOCK_SIZE;
      // larger part do alloc and write
      if (blockNum >= oldBlockNum)
        ino->blocks[blockNum] = bm->alloc_block();
      bm->write_block(ino->blocks[blockNum], blockStr);
    }
    
    // free the rest block
    else if (blockNum < oldBlockNum)
      bm->free_block(ino->blocks[blockNum]);
  }

  char IdirectBlockBuf[BLOCK_SIZE];//for indirect

  // do indirect block
  //if old < ndirect and new >ndirect alloc 
  if (oldBlockNum <= NDIRECT && newBlockNum > NDIRECT)
    ino->blocks[NDIRECT] = bm->alloc_block();
  bm->read_block(ino->blocks[NDIRECT], IdirectBlockBuf);
  blockid_t * indArr = (blockid_t *) IdirectBlockBuf;
  for (; blockNum < MAXFILE; blockNum++) {
    if (leftSize > 0) {
      writeSize = MIN(BLOCK_SIZE, leftSize);
      memcpy(blockStr, buf + blockNum * BLOCK_SIZE, writeSize);
      leftSize -= BLOCK_SIZE;
      // larger
      if (blockNum >= oldBlockNum)
        indArr[blockNum - NDIRECT] = bm->alloc_block();
      bm->write_block(indArr[blockNum - NDIRECT], blockStr);
    }
    // smaller
    else if (blockNum < oldBlockNum)
      bm->free_block(indArr[blockNum - NDIRECT]);
  }
  // free the rest block or write
  if (newBlockNum <= NDIRECT && oldBlockNum > NDIRECT)
    bm->free_block(ino->blocks[NDIRECT]);
  else
    bm->write_block(ino->blocks[NDIRECT], IdirectBlockBuf);

//change ino
  ino->size = size;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), inoStr);
  return;
}//right

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  
  if (inum <= 0 || inum >= INODE_NUM) {
    return;
  }
  struct inode *ino;
  char inoStr[BLOCK_SIZE];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inoStr);//not use get 
  ino = (struct inode*) inoStr;
  a.type = ino->type;
  a.size = ino->size;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */


  char blockStr[BLOCK_SIZE];
  //uint32_t blockNum;


  if (inum <= 0 || inum >= INODE_NUM) {
    return;
  }


  struct inode *ino;
  char inoStr[BLOCK_SIZE];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inoStr);
  ino = (struct inode*) inoStr;

  // do direct
  int leftSize = ino->size;
      //free is left
  for (uint32_t blockNum = 0; blockNum < NDIRECT; blockNum++) {
    if (leftSize <= 0)
      break;
    bm->free_block(ino->blocks[blockNum]);
    leftSize -= BLOCK_SIZE;
  }

  // do indirect
  if (leftSize > 0) {
    bm->read_block(ino->blocks[NDIRECT], blockStr);
    blockid_t *IndArr = (blockid_t *) blockStr;
    //free is left
    for (uint32_t blockNum = NDIRECT; blockNum < MAXFILE; blockNum++) {
      if (leftSize <= 0)
        break;
      bm->free_block(IndArr[blockNum - NDIRECT]);
      leftSize -= BLOCK_SIZE;
    }
    // free indirect block
    bm->free_block(ino->blocks[NDIRECT]);
  }
  
  free_inode(inum);
  return;
}//finish1145

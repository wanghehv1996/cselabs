#include "inode_manager.h"


//#define DEBUG

// First data block
 #define DBLOCK (BLOCK_NUM / BPB + INODE_NUM + 4)

// disk layer -----------------------------------------

// disk::disk()
// {
//   bzero(blocks, sizeof(blocks));
// }

// void
// disk::read_block(blockid_t id, char *buf)
// {
//   if(buf==NULL||id<0||id>=BLOCK_NUM)
//     return;

//   memcpy(buf, blocks[id], BLOCK_SIZE);
// }

// void
// disk::write_block(blockid_t id, const char *buf)
// {
//   if(buf==NULL||id<0||id>=BLOCK_NUM)
//     return;

//   memcpy(blocks[id], buf, BLOCK_SIZE);
// }


// block layer -----------------------------------------

/*
char decode(short word){
  char ret = 0;
  ret = word&0xff;
  return ret;
}

short encode(char word){
  short ret = 0;
  ret|=word;
  ret<<8;
  ret|=word;
  return ret;
}*/

short encode(char ch){
	short sh;
	sh = ch << 8;
	bool s[8];
	char mask = 0x01;
	for (int i = 0; i < 8; i++){
		s[7 - i] = !!(ch&mask);
		mask = mask << 1;
	}
	sh = sh | ((s[4] ^ s[5] ^ s[6] ^ s[7]) << 7);
	sh = sh | ((s[2] ^ s[3] ^ s[6] ^ s[7]) << 6);
	sh = sh | ((s[1] ^ s[3] ^ s[5] ^ s[7]) << 5);
	sh = sh | ((s[1] ^ s[2] ^ s[4] ^ s[6]) << 4);

	sh = sh | ((s[0] ^ s[3] ^ s[5] ^ s[6] ^ s[7]) << 3);
	sh = sh | ((s[0] ^ s[2] ^ s[4] ^ s[7]) << 2);
	sh = sh | ((s[0] ^ s[1] ^ s[3] ^ s[4] ^ s[5] ^ s[6]) << 1);
	sh = sh | (s[0] ^ s[1] ^ s[2] ^ s[5] ^ s[6] ^ s[7]);
	
	return sh;
}

char decode(short sh){
	char ch = 0;
	bool s[16];
	short check = 0;
	short mask = 0x01;
	ch = sh >> 8;
	for (int i = 0; i < 16; i++){
		s[15 - i] = !!(sh&mask);
		mask = mask << 1;
	}

	//right 0 false 1
	check = check | ((s[4] ^ s[5] ^ s[6] ^ s[7] ^ s[8]) << 7);
	check = check | ((s[2] ^ s[3] ^ s[6] ^ s[7] ^ s[9]) << 6);
	check = check | ((s[1] ^ s[3] ^ s[5] ^ s[7] ^ s[10]) << 5);
	check = check | ((s[1] ^ s[2] ^ s[4] ^ s[6] ^ s[11]) << 4);
	check = check | ((s[0] ^ s[3] ^ s[5] ^ s[6] ^ s[7] ^ s[12]) << 3);
	check = check | ((s[0] ^ s[2] ^ s[4] ^ s[7] ^ s[13]) << 2);
	check = check | ((s[0] ^ s[1] ^ s[3] ^ s[4] ^ s[5] ^ s[6] ^ s[14]) << 1);
	check = check | (s[0] ^ s[1] ^ s[2] ^ s[5] ^ s[6] ^ s[7] ^ s[15]);

	switch (check){
	case 0:case 0x1:case 0x2:case 0x4:case 0x8: case 0x10: case 0x20:case 0x40:case 0x80:
		return ch;
	case 0xe:
	case 0xd:
	case 0xb:
	case 0x7:
	case 0x1f:
	case 0x2f:
	case 0x4f:
	case 0x8f:
	case 0xf:
		ch ^= ((1 << 7));
		return ch;
	case 0x32:
	case 0x31:
	case 0x37:
	case 0x3b:
	case 0x23:
	case 0x13:
	case 0x73:
	case 0xb3:
		ch ^= ((1 << 6));
		return ch;
	case 0x3c:
		ch ^= ((1 << 6) ^ (1 << 7));
		return ch;
	case 0x33:
		ch ^= ((1 << 6));
		return ch;
	case 0x54:
	case 0x57:
	case 0x51:
	case 0x5d:
	case 0x45:
	case 0x75:
	case 0x15:
	case 0xd5:
		ch ^= ((1 << 5));
		return ch;
	case 0x5a:
		ch ^= ((1 << 5) ^ (1 << 7));
		return ch;
	case 0x66:
		ch ^= ((1 << 5) ^ (1 << 6));
		return ch;
	case 0x55:
		ch ^= ((1 << 5));
		return ch;
	case 0x6b:
	case 0x68:
	case 0x6e:
	case 0x62:
	case 0x7a:
	case 0x4a:
	case 0x2a:
	case 0xea:
		ch ^= ((1 << 4));
		return ch;
	case 0x65:
		ch ^= ((1 << 4) ^ (1 << 7));
		return ch;
	case 0x59:
		ch ^= ((1 << 4) ^ (1 << 6));
		return ch;
	case 0x3f:
		ch ^= ((1 << 4) ^ (1 << 5));
		return ch;
	case 0x6a:
		ch ^= ((1 << 4));
		return ch;
	case 0x97:
	case 0x94:
	case 0x92:
	case 0x9e:
	case 0x86:
	case 0xb6:
	case 0xd6:
	case 0x16:
		ch ^= ((1 << 3));
		return ch;
	case 0x99:
		ch ^= ((1 << 3) ^ (1 << 7));
		return ch;
	case 0xa5:
		ch ^= ((1 << 3) ^ (1 << 6));
		return ch;
	case 0xc3:
		ch ^= ((1 << 3) ^ (1 << 5));
		return ch;
	case 0xfc:
		ch ^= ((1 << 3) ^ (1 << 4));
		return ch;
	case 0x96:
		ch ^= ((1 << 3));
		return ch;
	case 0xaa:
	case 0xa9:
	case 0xaf:
	case 0xa3:
	case 0xbb:
	case 0x8b:
	case 0xeb:
	case 0x2b:
		ch ^= ((1 << 2));
		return ch;
	case 0xa4:
		ch ^= ((1 << 2) ^ (1 << 7));
		return ch;
	case 0x98:
		ch ^= ((1 << 2) ^ (1 << 6));
		return ch;
	case 0xfe:
		ch ^= ((1 << 2) ^ (1 << 5));
		return ch;
	case 0xc1:
		ch ^= ((1 << 2) ^ (1 << 4));
		return ch;
	case 0x3d:
		ch ^= ((1 << 2) ^ (1 << 3));
		return ch;
	case 0xab:
		ch ^= ((1 << 2));
		return ch;
	case 0xda:
	case 0xd9:
	case 0xdf:
	case 0xd3:
	case 0xcb:
	case 0xfb:
	case 0x9b:
	case 0x5b:
		ch ^= ((1 << 1));
		return ch;
	case 0xd4:
		ch ^= ((1 << 1) ^ (1 << 7));
		return ch;
	case 0xe8:
		ch ^= ((1 << 1) ^ (1 << 6));
		return ch;
	case 0x8e:
		ch ^= ((1 << 1) ^ (1 << 5));
		return ch;
	case 0xb1:
		ch ^= ((1 << 1) ^ (1 << 4));
		return ch;
	case 0x4d:
		ch ^= ((1 << 1) ^ (1 << 3));
		return ch;
	case 0x70:
		ch ^= ((1 << 1) ^ (1 << 2));
		return ch;
	case 0xdb:
		ch ^= ((1 << 1));
		return ch;
	case 0xec:
	case 0xef:
	case 0xe9:
	case 0xe5:
	case 0xfd:
	case 0xcd:
	case 0xad:
	case 0x6d:
		ch ^= ((1 << 0));
		return ch;
	case 0xe2:
		ch ^= ((1 << 0) ^ (1 << 7));
		return ch;
	case 0xde:
		ch ^= ((1 << 0) ^ (1 << 6));
		return ch;
	case 0xb8:
		ch ^= ((1 << 0) ^ (1 << 5));
		return ch;
	case 0x87:
		ch ^= ((1 << 0) ^ (1 << 4));
		return ch;
	case 0x7b:
		ch ^= ((1 << 0) ^ (1 << 3));
		return ch;
	case 0x46:
		ch ^= ((1 << 0) ^ (1 << 2));
		return ch;
	case 0x36:
		ch ^= ((1 << 0) ^ (1 << 1));
		return ch;
	case 0xed:
		ch ^= ((1 << 0));
		return ch;

	}

	return ch;
}

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
    d->read_block(BBLOCK(blockId), blockStr);

    // if free
    if ((~(blockStr[off / 8] | (~(1 << (off % 8))))) != 0) {
      blockStr[off / 8] |= 1 << (off % 8);
      d->write_block(BBLOCK(blockId), blockStr);
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
  d->read_block(BBLOCK(id), buf);

  //free block in bitmap
  buf[off / 8] &= ~(1 << (off % 8));

  d->write_block(BBLOCK(id), buf);
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
  d->write_block(1, buf);

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  char buf0[BLOCK_SIZE];
  char halfbuf0[BLOCK_SIZE/2];
  d->read_block(id, buf0);
  for(int j = 0;j<BLOCK_SIZE/2;j++){
    ((char*)halfbuf0)[j] = decode(((short*)buf0)[j]);
  }
  memcpy(buf,halfbuf0, BLOCK_SIZE/2);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  char buf0[BLOCK_SIZE];
  
  //char halfbuf0[BLOCK_SIZE/2];
  //memcpy(halfbuf0 ,buf, BLOCK_SIZE/2);
  for(int j = 0;j<BLOCK_SIZE/2;j++){
    ((short*)buf0)[j] = encode(((char*)buf)[j]);
  }
  d->write_block(id, buf0);
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
  //char inodeBuf[BLOCK_SIZE];
  char halfInoStr[BLOCK_SIZE/2];

  for (uint32_t inum = 1; inum <= INODE_NUM; inum++) {
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);//dont use inode function here
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((short*)halfInoStr)[j] = decode(((int*)inodeBuf)[j]);
//    }
    ino = (struct inode*) halfInoStr;
    if (ino->type == 0) {
      ino->type = type;
      ino->size = 0;
      ino->ctime = ino->mtime = ino->atime = time(NULL);
//      for(int j = 0;j<BLOCK_SIZE/4;j++){
//        ((int*)inodeBuf)[j] = encode(((short*)halfInoStr)[j]);
//      }
      bm->write_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
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

  //char inodeBuf[BLOCK_SIZE];
  char halfInoStr[BLOCK_SIZE/2];
  struct inode *ino;
  if (inum <= 0 || inum >= INODE_NUM) 
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);//dont use inode function here
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)inodeBuf)[j]);
//  }
  ino = (struct inode*) halfInoStr;//change type 
  ino->type = 0;
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//     ((int*)inodeBuf)[j] = encode(((short*)halfInoStr)[j]);
//  }
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  //char buf[BLOCK_SIZE];
  char halfInoStr[BLOCK_SIZE/2];

  printf("\tim: get_inode %d\n", inum);

  // modified, "inum < 0" ==> "inum <= 0"
  if (inum <= 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)buf)[j]);
//  }
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)halfInoStr + inum%IPB;
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
  //char buf[BLOCK_SIZE];
  char halfInoStr[BLOCK_SIZE/2];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)buf)[j]);
//  }
  ino_disk = (struct inode*)halfInoStr + inum%IPB;
  *ino_disk = *ino;
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//     ((int*)buf)[j] = encode(((short*)halfInoStr)[j]);
//  }
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
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
  char halfInoStr[BLOCK_SIZE/2];
  char blockStr[BLOCK_SIZE];
  char halfBlockStr[BLOCK_SIZE/2];
  char *retBuf;



  if (inum <= 0 || inum >= INODE_NUM) {
    return;
  }

//change access time
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)inoStr)[j]);
//  }
  ino = (struct inode*) halfInoStr;
  ino->atime = time(NULL);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//     ((int*)inoStr)[j] = encode(((short*)halfInoStr)[j]);
//  }
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);

//do sth
  if (ino->size == 0)
    return;
  *size = ino->size;
  retBuf = (char *) malloc(ino->size);
  *buf_out = retBuf;


//
  int leftSize = ino->size;
  int readSize = BLOCK_SIZE;
  
// for direct case change ndir
  for (int i = 0; i < NDIRECT-1; i++) {
    if (leftSize <= 0)
      return; 
    readSize = MIN(BLOCK_SIZE, leftSize*2);// *2
    bm->read_block(ino->blocks[i], halfBlockStr);
    leftSize -= BLOCK_SIZE/2;// /2
    
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((short*)halfBlockStr)[j] = decode(((int*)blockStr)[j]);
//    }
    
    //memcpy(retBuf + i * BLOCK_SIZE, blockStr, readSize);
    memcpy(retBuf + i * BLOCK_SIZE/2, halfBlockStr, readSize/2);
  }


  char IdirectBlockBuf[BLOCK_SIZE];//for indirect
  char halfIdirectBlockBuf[BLOCK_SIZE/2];

  blockid_t *indArr;
// for indirect case
  bm->read_block(ino->blocks[NDIRECT-1], halfIdirectBlockBuf);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfIdirectBlockBuf)[j] = decode(((int*)IdirectBlockBuf)[j]);
//  }
  indArr = (blockid_t *) halfIdirectBlockBuf;
  for (uint i = 0; i < NINDIRECT/2; i++) {// /2
    if (leftSize <= 0)
      return;
    readSize = MIN(BLOCK_SIZE, leftSize*2);// *2
    bm->read_block(indArr[i], halfBlockStr);
    leftSize -= BLOCK_SIZE/2;// /2
    
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((short*)halfBlockStr)[j] = decode(((int*)blockStr)[j]);
//    }
    
    //memcpy(retBuf + (NDIRECT + i) * BLOCK_SIZE, blockStr, readSize);
    memcpy(retBuf + (NDIRECT - 1 + i) * BLOCK_SIZE/2, halfBlockStr, readSize/2);
  }
  
  
  bm->read_block(ino->blocks[NDIRECT], halfIdirectBlockBuf);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfIdirectBlockBuf)[j] = decode(((int*)IdirectBlockBuf)[j]);
//  }
  indArr = (blockid_t *) halfIdirectBlockBuf;
  for (uint i = 0; i < NINDIRECT/2; i++) {// /2
    if (leftSize <= 0)
      return;
    readSize = MIN(BLOCK_SIZE, leftSize*2);// *2
    bm->read_block(indArr[i], halfBlockStr);
    leftSize -= BLOCK_SIZE/2;// /2
    
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((short*)halfBlockStr)[j] = decode(((int*)blockStr)[j]);
//    }
    
    //memcpy(retBuf + (NDIRECT + i) * BLOCK_SIZE, blockStr, readSize);
    memcpy(retBuf + (NDIRECT -1 +NINDIRECT/2 + i) * BLOCK_SIZE/2, halfBlockStr, readSize/2);
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
  char halfInoStr[BLOCK_SIZE/2];
  char blockStr[BLOCK_SIZE];
  char halfBlockStr[BLOCK_SIZE/2];
  

  int blockNum;

  if (inum <= 0 || inum >= INODE_NUM||size < 0 || size > MAXFILE * BLOCK_SIZE) {
    return;
  }

//get inode
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)inoStr)[j]);
//  }
  ino = (struct inode*) halfInoStr;

  
  
  ino->mtime = time(NULL);
  if (size != ino->size)
      ino->ctime = ino->mtime;

//get block num
  int oldBlockNum = (ino->size==0) ? 0 :((int)(ino->size*2) - 1) / BLOCK_SIZE+1;// *2
  int newBlockNum = (size*2 - 1) / BLOCK_SIZE+1;// *2

  //do direct blocks
  int leftSize = size;
  int writeSize = BLOCK_SIZE;
  for (blockNum = 0; blockNum < NDIRECT-1; blockNum++) {
    if (leftSize > 0) {
      writeSize = MIN(BLOCK_SIZE, leftSize*2);// *2
      //write to right posi
      //memcpy(blockStr, buf + blockNum * BLOCK_SIZE, writeSize);
      memcpy(halfBlockStr, buf + blockNum * BLOCK_SIZE/2, writeSize/2);// /2
      
//      for(int j = 0;j<BLOCK_SIZE/4;j++){
//        ((int*)blockStr)[j] = encode(((short*)halfBlockStr)[j]);
//      }
      leftSize -= BLOCK_SIZE/2;// /2
      // larger part do alloc and write
      if (blockNum >= oldBlockNum)
        ino->blocks[blockNum] = bm->alloc_block();
      bm->write_block(ino->blocks[blockNum], halfBlockStr);
    }
    // free the rest block
    else if (blockNum < oldBlockNum)
      bm->free_block(ino->blocks[blockNum]);
  }

  char IdirectBlockBuf[BLOCK_SIZE];//for indirect
  char halfIdirectBlockBuf[BLOCK_SIZE/2];

  // do indirect block
  //if old < ndirect and new >ndirect alloc 
  if (oldBlockNum < NDIRECT && newBlockNum >= NDIRECT)
    ino->blocks[NDIRECT-1] = bm->alloc_block();
  bm->read_block(ino->blocks[NDIRECT-1], halfIdirectBlockBuf);
  
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfIdirectBlockBuf)[j] = decode(((int*)IdirectBlockBuf)[j]);
//  }
  
  blockid_t * indArr = (blockid_t *) halfIdirectBlockBuf;
  for (; blockNum < NDIRECT-1+NINDIRECT/2; blockNum++) {
    if (leftSize > 0) {
      writeSize = MIN(BLOCK_SIZE, leftSize*2);// *2
      //memcpy(blockStr, buf + blockNum * BLOCK_SIZE, writeSize);
      memcpy(halfBlockStr, buf + blockNum * BLOCK_SIZE/2, writeSize/2);
      leftSize -= BLOCK_SIZE/2;// /2
      
//      for(int j = 0;j<BLOCK_SIZE/4;j++){
//        ((int*)blockStr)[j] = encode(((short*)halfBlockStr)[j]);
//      }
      
      // larger
      if (blockNum >= oldBlockNum)
        indArr[blockNum - NDIRECT+1] = bm->alloc_block();
      bm->write_block(indArr[blockNum - NDIRECT +1], halfBlockStr);
    }
    // smaller
    else if (blockNum < oldBlockNum)
      bm->free_block(indArr[blockNum - NDIRECT+1]);
  }
  // free the rest block or write
  if (newBlockNum < NDIRECT && oldBlockNum >= NDIRECT)
    bm->free_block(ino->blocks[NDIRECT-1]);
  else{
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((int*)IdirectBlockBuf)[j] = encode(((short*)halfIdirectBlockBuf)[j]);
//    }
    bm->write_block(ino->blocks[NDIRECT-1], halfIdirectBlockBuf);
  }
  
    // do indirect block
  //if old < ndirect and new >ndirect alloc 
  if (oldBlockNum < NDIRECT+NINDIRECT/2 && newBlockNum >= NDIRECT+NINDIRECT/2)
    ino->blocks[NDIRECT] = bm->alloc_block();
  bm->read_block(ino->blocks[NDIRECT], halfIdirectBlockBuf);
  
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfIdirectBlockBuf)[j] = decode(((int*)IdirectBlockBuf)[j]);
//  }
  
  indArr = (blockid_t *) halfIdirectBlockBuf;
  for (; blockNum < NDIRECT-1+NINDIRECT; blockNum++) {
    if (leftSize > 0) {
      writeSize = MIN(BLOCK_SIZE, leftSize*2);// *2
      //memcpy(blockStr, buf + blockNum * BLOCK_SIZE, writeSize);
      memcpy(halfBlockStr, buf + blockNum * BLOCK_SIZE/2, writeSize/2);
      leftSize -= BLOCK_SIZE/2;// /2
      
//      for(int j = 0;j<BLOCK_SIZE/4;j++){
//        ((int*)blockStr)[j] = encode(((short*)halfBlockStr)[j]);
//      }
      
      // larger
      if (blockNum >= oldBlockNum)
        indArr[blockNum - NDIRECT+1-NINDIRECT/2] = bm->alloc_block();
      bm->write_block(indArr[blockNum - NDIRECT+1-NINDIRECT/2], halfBlockStr);
    }
    // smaller
    else if (blockNum < oldBlockNum)
      bm->free_block(indArr[blockNum - NDIRECT+1-NINDIRECT/2]);
  }
  // free the rest block or write
  if (newBlockNum < NDIRECT+NINDIRECT/2 && oldBlockNum >= NDIRECT+NINDIRECT/2)
    bm->free_block(ino->blocks[NDIRECT]);
  else{
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((int*)IdirectBlockBuf)[j] = encode(((short*)halfIdirectBlockBuf)[j]);
//    }
    bm->write_block(ino->blocks[NDIRECT], halfIdirectBlockBuf);
  }
  
//change ino
  ino->size = size;
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//     ((int*)inoStr)[j] = encode(((short*)halfInoStr)[j]);
//  }

  bm->write_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);
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
  char halfInoStr[BLOCK_SIZE/2];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);//not use get 
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)inoStr)[j]);
//  }
  ino = (struct inode*) halfInoStr;
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
  char halfBlockStr[BLOCK_SIZE/2];
  //uint32_t blockNum;


  if (inum <= 0 || inum >= INODE_NUM) {
    return;
  }


  struct inode *ino;
  char inoStr[BLOCK_SIZE];
  char halfInoStr[BLOCK_SIZE/2];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), halfInoStr);//not use get 
//  for(int j = 0;j<BLOCK_SIZE/4;j++){
//    ((short*)halfInoStr)[j] = decode(((int*)inoStr)[j]);
//  }
  ino = (struct inode*) halfInoStr;

  // do direct
  int leftSize = ino->size*2;// *2
      //free is left
  for (uint32_t blockNum = 0; blockNum < NDIRECT-1; blockNum++) {
    if (leftSize <= 0)
      break;
    bm->free_block(ino->blocks[blockNum]);
    leftSize -= BLOCK_SIZE;
  }

  // do indirect
  if (leftSize > 0) {
    bm->read_block(ino->blocks[NDIRECT-1], halfBlockStr);
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((short*)halfBlockStr)[j] = decode(((int*)blockStr)[j]);
//    }
    blockid_t *IndArr = (blockid_t *) halfBlockStr;
    //free is left
    for (uint32_t blockNum = 0; blockNum < NINDIRECT/2; blockNum++) {
      if (leftSize <= 0)
        break;
      bm->free_block(IndArr[blockNum]);
      leftSize -= BLOCK_SIZE;
    }
    // free indirect block
    bm->free_block(ino->blocks[NDIRECT-1]);
  }
  
  if (leftSize > 0) {
    bm->read_block(ino->blocks[NDIRECT], halfBlockStr);
//    for(int j = 0;j<BLOCK_SIZE/4;j++){
//      ((short*)halfBlockStr)[j] = decode(((int*)blockStr)[j]);
//    }
    blockid_t *IndArr = (blockid_t *) halfBlockStr;
    //free is left
    for (uint32_t blockNum = 0; blockNum < NINDIRECT/2; blockNum++) {
      if (leftSize <= 0)
        break;
      bm->free_block(IndArr[blockNum]);
      leftSize -= BLOCK_SIZE;
    }
    // free indirect block
    bm->free_block(ino->blocks[NDIRECT]);
  }
  
  free_inode(inum);
  return;
}//finish1145

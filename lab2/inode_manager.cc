#include "inode_manager.h"
#include <ctime>

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
   */
	char blockStr[BLOCK_SIZE], mask;
	//check it one byte by one byte
	//dont use using map
	for (blockid_t blockIdA = 0; blockIdA < BLOCK_NUM; blockIdA += BPB) {
		//get the block of bitmap
		blockid_t bmId = BBLOCK(blockIdA);
		read_block(bmId, blockStr);
		uint32_t off = 0;
		//do it while in size
		while((off < BPB) && (blockIdA + off < BLOCK_NUM) ){
			mask = 1 << (off % 8);
			//find free block
			if ((blockStr[off/8] & mask) == 0) {
				blockStr[off/8] |= mask;
				write_block(BBLOCK(blockIdA), blockStr);
				return blockIdA + off;
			}
			off++;
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
   
   //jump some blocks
   
  if (id < 2 + BLOCK_NUM/BPB + INODE_NUM/IPB) {
		return;
	}
	char blockStr[BLOCK_SIZE], mask;
  blockid_t bmId = BBLOCK(id);
	read_block(bmId, blockStr);
	uint32_t bmCharOffset = (id % BPB) / 8;
  uint32_t bmBitOffset = (id % BPB) % 8;
	mask = 1 << (bmBitOffset);
	blockStr[bmCharOffset] &= (~mask);
	write_block(bmId, blockStr);
	return;
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

	// write superblock to disk's block[1]
	char buf[BLOCK_SIZE];
	memset(buf, 0, BLOCK_SIZE);
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
   
   //shouldnt use get inode!!!
	char inodeBuf[BLOCK_SIZE];

	for (uint32_t inum = 1; inum < INODE_NUM; inum++) {
	
		memset(inodeBuf, 0, BLOCK_SIZE);
		bm->read_block(IBLOCK(inum, BLOCK_NUM), inodeBuf);
		inode_t* ino = (inode_t*)inodeBuf + inum % IPB;
		
		//blank
		if (ino->type == 0) {
			ino->type = type;
			ino->ctime = ino->atime = ino->mtime = time(NULL);
			put_inode(inum,ino);
			//bm->write_block(IBLOCK(inum, BLOCK_NUM), inodeBuf);
			return inum;
		}
	}
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
	inode_t* ino = get_inode(inum);
	if(ino==NULL) return;
	if (ino->type == 0)
	  return;
	memset(ino, 0, sizeof(inode_t));
	//ino->type = 0;
	//inode->ctime = inode->atime = inode->mtime = 0;
	put_inode(inum, ino);
	free(ino);
  return;
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

 char blockStr[BLOCK_SIZE];
 char IdirectBlockBuf[BLOCK_SIZE];

 inode_t* inode = get_inode(inum);
 if (inode == NULL) {
   exit(0);
 }
 //dont care if is read successfully
 *size = inode->size;
 
 char* retBuf = (char*)malloc(inode->size);	// free by caller
 *buf_out = retBuf;	// bind buf_out with new malloc area
 memset(retBuf, 0, inode->size);

 // judge whether need indirect block
 bool need_indirect = (inode->size > (NDIRECT * BLOCK_SIZE));
 uint32_t* indArr;
 if (need_indirect) {
   bm->read_block(inode->blocks[NDIRECT], IdirectBlockBuf);
   indArr = (uint32_t*)IdirectBlockBuf;
 }

 // have readed
 uint32_t loadSize = 0;	
 // left size
 uint32_t leftSize = inode->size; 
 // readsize onetime
 uint32_t readSize = 0; 
// as iterator
 uint32_t blockNum = 0;	

 for(loadSize = 0; leftSize > 0; leftSize -= readSize, loadSize += readSize) {
   readSize = MIN(BLOCK_SIZE, leftSize);
   // for indirect case
   if (blockNum >= NDIRECT) {
     bm->read_block(indArr[blockNum-NDIRECT], blockStr);
   }
   // for direct case
   else {
     bm->read_block(inode->blocks[blockNum], blockStr);
   }
   
   memcpy(retBuf, blockStr, readSize);
   retBuf += readSize;
   blockNum++;
 }
 // change time
 inode->atime = time(NULL);
 put_inode(inum, inode);
 free(inode);
 //dont free str here  will cause segment fault
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

	inode_t* ino = get_inode(inum);
	bool oldIndirect = ((ino->size/BLOCK_SIZE) > NDIRECT);
	uint32_t oldSize = ino->size;
	uint32_t oldBlockNum = (oldSize==0) ? 0 : (oldSize-1) / BLOCK_SIZE + 1;

	ino->size = MIN(MAXFILE *BLOCK_SIZE, (unsigned)size);

	char IdirectBlockBuf[BLOCK_SIZE];//for indirect
	char writeBuf[BLOCK_SIZE];//for write one time
	
	uint32_t* indArr;
	uint32_t newBlockSize = (ino->size==0) ? 0 : ino->size / BLOCK_SIZE + 1;
	uint32_t doneSize = 0, writeSize = 0;
	uint32_t blockNum = 0, newBlockNum = 0;
	uint32_t leftSize = ino->size;
	bool needInd = (newBlockSize > NDIRECT);




	// free the old indirect blocks
	if (oldIndirect) {
		uint32_t indBlock = ino->blocks[NDIRECT];
		char oldData[BLOCK_SIZE];
		bm->read_block(indBlock, oldData);
		uint32_t* oldIndArr = (uint32_t*)oldData;
		for (uint32_t i = 0; i < oldBlockNum - NDIRECT; i++)
			bm->free_block(oldIndArr[i]);
		bm->free_block(indBlock);
	}
	// free old direct block
	for (uint32_t i = 0; i < oldBlockNum && i < NDIRECT; i++)
		bm->free_block(ino->blocks[i]);

	// init indirect block
	if (needInd) {
		ino->blocks[NDIRECT] = bm->alloc_block();
		bm->read_block(ino->blocks[NDIRECT], IdirectBlockBuf);
		indArr = (uint32_t*)IdirectBlockBuf;
	}

	//begin write
	for (blockNum = 0; blockNum < newBlockSize; blockNum++) {
		writeSize = MIN(leftSize, BLOCK_SIZE);
		// do NDIRECT
		if (blockNum < NDIRECT) {
			newBlockNum = bm->alloc_block();
			ino->blocks[blockNum] = newBlockNum;
		}
		// do NINDIRECT
		else {
			newBlockNum = bm->alloc_block();
			indArr[blockNum - NDIRECT] = newBlockNum;
			bm->write_block(ino->blocks[NDIRECT], IdirectBlockBuf);
		}

		memset(writeBuf, 0, BLOCK_SIZE);
		memcpy(writeBuf, buf, writeSize);
		bm->write_block(newBlockNum, writeBuf);
		buf += writeSize;
		doneSize += writeSize;
		leftSize -= writeSize;
	}

	ino->ctime = time(NULL);
  ino->mtime = time(NULL);
	put_inode(inum, ino);
	free(ino);
  return;

}


void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
	inode_t* inode = get_inode(inum);
	if (inode == NULL) {
		a.type = a.size = a.atime = a.mtime = a.ctime = 0;
	} else {
		a.type = inode->type;
		a.size = inode->size;
		a.atime = inode->atime;
		a.mtime = inode->mtime;
		a.ctime = inode->ctime;
	}
	free(inode);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */
	
	inode_t* inode = get_inode(inum);
	uint32_t size = inode->size;
	uint32_t total_blocks = size / BLOCK_SIZE + 1;
	if (size % BLOCK_SIZE == 0) total_blocks--;
	
	// free old indirect block
	if ((inode->size/BLOCK_SIZE) > NDIRECT) { 
		uint32_t indirect_block = inode->blocks[NDIRECT];
		char old_buffer[BLOCK_SIZE];
		bm->read_block(indirect_block, old_buffer);
		uint32_t* old_indirect_array = (uint32_t*)old_buffer;
		for (uint32_t i = 0; i < total_blocks - NDIRECT; i++)
			bm->free_block(old_indirect_array[i]);	// free indirected's data block
		bm->free_block(indirect_block);						// free indirected's index block
	}
	// free old direct block
	for (uint32_t i = 0; i < total_blocks && i < NDIRECT; i++)
		bm->free_block(inode->blocks[i]);
	// free inode table and get_inode's malloc 
	free_inode(inum);
	free(inode);
  return;
}

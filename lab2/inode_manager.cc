#include "inode_manager.h"
#include <string.h>
#include <time.h>

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
  if (id < 0 || id > BLOCK_NUM || buf == NULL) {
    return;
  }
  memcpy(buf, blocks[id], BLOCK_SIZE);
  return;
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
  if (id < 0 || id > BLOCK_NUM || buf == NULL) {
    return;
  }
  memcpy(blocks[id], buf, BLOCK_SIZE);
  return;
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
  char buf[BLOCK_SIZE]; // used to read from the free block bitmap

  // the first block that can be allocated
  blockid_t start = IBLOCK(sb.ninodes, sb.nblocks) + 1;
  for (unsigned int i = start; i < sb.nblocks; i += BPB) {
    read_block(BBLOCK(i), buf);
    for (unsigned int j = 0; 
            (j < BLOCK_SIZE) && (i + j < sb.nblocks); j++) {
      char cur = buf[j];
      for (unsigned int k = 0; (k < 8) && (i + j + k < sb.nblocks);
              k++) {
        if ((cur & (0x1 << k)) == 0) {
          buf[j] |= (0x1 << k);
          write_block(BBLOCK(i), buf);
          // printf("alloc: %d, cur: 0x%x, i: %d, j: %d, k: %d, 0x%x \n", i+j*8+k, cur, i, j, k, 0x1<< k);
          return i + j*8 + k;
        }
      }
    }
  }

  // no block available
  exit(0);
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  read_block(BBLOCK(id), buf);
  int index = (id % BPB) / 8;
  char mask = ~(0x1 << ((id % BPB) % 8));
  buf[index] = buf[index] & mask;
  write_block(BBLOCK(id), buf);
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
  // printf("write: %d, %s \n", id, buf);
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
  // search from the 2nd inode to the last one to find a free inode.
  for (unsigned int i = 1; i < bm->sb.ninodes; i++) {
    struct inode* ino = get_inode(i);
    // if ino == NULL, then this is a free inode
    if (ino == NULL) {
      struct inode newino;
      newino.type = type;
      newino.size = 0;
      newino.atime = newino.mtime = newino.ctime = time(0);
      put_inode(i, &newino);
      return i;
    }
    else {
      free(ino);
    }
  }
  // All inodes have been used
  exit(0);
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
  
  struct inode* ino = get_inode(inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return;
  }

  if (ino == NULL) {
    return;
  }

  memset(ino, 0, sizeof(inode_t));
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

  // printf("\tim: get_inode %d\n", inum);

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
  if (inum <= 0 || inum >= bm->sb.ninodes) {
    // printf("inum out of range!\n");
    return;
  }
  struct inode* ino = get_inode(inum);
  if (ino == NULL) {
    // printf("inode doesn't exist!\n");
    return;
  }
  *size = ino->size;
  char *buf = (char *)malloc(ino->size);
  *buf_out = buf;
  char block[BLOCK_SIZE];
  int rest = ino->size;
  // if the file is not large enough to use indirect block
  if (*size <= BLOCK_SIZE*NDIRECT) {
    int i = 0;
    while (rest > 0) {
      bm->read_block(ino->blocks[i], block);
      unsigned int len = MIN(rest, BLOCK_SIZE);
      memcpy(buf + i * BLOCK_SIZE, block, len);
      // printf("inum: %d, size: %d, %s, i: %d \n", 
      //         inum, *size, block, i);
      rest -= BLOCK_SIZE;
      i++;
    }
  }
  // if the file is large enough to use indirect block
  else {
    // deal with direct blocks
    for (int i = 0; i < NDIRECT; i++) {
      bm->read_block(ino->blocks[i], block);
      memcpy(buf + i * BLOCK_SIZE, block, BLOCK_SIZE);
    }
    rest -= BLOCK_SIZE*NDIRECT;
    // deal with indirect block
    bm->read_block(ino->blocks[NDIRECT], block);
    blockid_t *blocks = (blockid_t *)calloc((BLOCK_SIZE / sizeof(blockid_t)), sizeof(blockid_t));
    memcpy(blocks, block, BLOCK_SIZE);
    int i = 0;
    while (rest > 0) {
      bm->read_block(blocks[i], block);
      // printf("bi: %d, %d, %s\n", blocks[i], i, block);
      int len = MIN(rest, BLOCK_SIZE);
      memcpy(buf + BLOCK_SIZE*NDIRECT + i*BLOCK_SIZE, block, len);
      rest -= BLOCK_SIZE;
      i++;
    }
    free(blocks);
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
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
  // check if the size is legal
  if (size < 0 || (unsigned int)size > MAXFILE * BLOCK_SIZE) {
    return;
  }
  struct inode* ino = get_inode(inum);
  char block[BLOCK_SIZE];
  unsigned int osize = ino->size;
  unsigned int onum = (osize % BLOCK_SIZE == 0) ?
      osize / BLOCK_SIZE : osize / BLOCK_SIZE + 1;
  unsigned int num = (size % BLOCK_SIZE == 0) ?
      size / BLOCK_SIZE : size / BLOCK_SIZE + 1;
  // deal with freeing or allocating blocks
  // freeing blocks
  if (onum > num) {
    if (num <= NDIRECT) {
      if (onum <= NDIRECT) {
        for (unsigned int i = num; i < onum; i++) {
          bm->free_block(ino->blocks[i]);
        }
      }
      else {
        bm->read_block(ino->blocks[NDIRECT], block);
        blockid_t *blocks = (blockid_t *) block;
        for (unsigned int i = 0; i < onum - NDIRECT; i++) {
          bm->free_block(blocks[i]);
        }
        for (unsigned int i = num; i < NDIRECT; i++) {
          bm->free_block(ino->blocks[i]);
        }
      }
    }
    else {
      bm->read_block(ino->blocks[NDIRECT], block);
      blockid_t *blocks = (blockid_t *) block;
      for (unsigned int i = (num - NDIRECT); i < (onum - NDIRECT); i++) {
        bm->free_block(blocks[i]);
      }
    }
  }
  // allocating new blocks
  else if (onum < num) {
    if (num <= NDIRECT) {
      for (unsigned int i = onum; i < num; i++) {
        ino->blocks[i] = bm->alloc_block();
      }
    }
    else {
      if (onum <= NDIRECT) {
        for (unsigned int i = onum; i < NDIRECT; i++) {
          ino->blocks[i] = bm->alloc_block();
        }
        ino->blocks[NDIRECT] = bm->alloc_block();
        blockid_t blocks[NDIRECT];
        for (unsigned int i = 0; i < num-NDIRECT; i++) {
          blocks[i] = bm->alloc_block();
          // printf("b[%d]: %d\n", i, blocks[i]);
        }
        bm->write_block(ino->blocks[NDIRECT], (char *)blocks);
      }
      else {
        bm->read_block(ino->blocks[NDIRECT], block);
        blockid_t *blocks = (blockid_t *) block;
        for (unsigned int i = (onum - NDIRECT); i < (num - NDIRECT); i++) {
          blocks[i] = bm->alloc_block();
        }
        bm->write_block(ino->blocks[NDIRECT], (char *) blocks);
      }
    }
  }

  // write the file
  int rest = size;
  if (num <= NDIRECT) {
    for (unsigned int i = 0; i < num; i++) {
      bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);     
    }
  }
  else {
    // printf("writing: \n%s\n", buf);
    for (unsigned int i = 0; i < NDIRECT; i++) {
      bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);     
    }
    blockid_t blocks[NINDIRECT];
    bm->read_block(ino->blocks[NDIRECT], (char *)blocks);
    // here a bug which passed the test in lab1 but bothers me in lab2
    // the "blocks" should just be another array but not "block", because
    // "block" will be modified later.
    // bm->read_block(ino->blocks[NDIRECT], block); 
    // blockid_t *blocks = (blockid_t *) block;
    rest = size - NDIRECT*BLOCK_SIZE;
    int i = 0;
    while (rest > 0) {
      if (rest >= BLOCK_SIZE) {
        bm->write_block(blocks[i], buf + (i + NDIRECT) * BLOCK_SIZE);
        // char tmp[BLOCK_SIZE];
        // bm->read_block(blocks[i], tmp);
        // printf("bi: %d, %s\n", blocks[i], tmp);
      }
      else {
        bzero(block, sizeof(block));
        memcpy(block, buf + (i + NDIRECT) * BLOCK_SIZE, rest);
        bm->write_block(blocks[i], block);
        // printf("bi: %d, %s\n", block);
      }
      i++;
      rest -= BLOCK_SIZE;
    }
  }
  
  // update the inode
  ino->size = size;
  ino->mtime = time(0);
  ino->ctime = time(0);
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
  struct inode* ino = get_inode(inum);
  if (ino != NULL) {
    a.type = ino->type;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    a.size = ino->size;
    free(ino);
  }
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

   // free the data block
   struct inode* ino = get_inode(inum);
   char buf[BLOCK_SIZE];
   unsigned int size = ino->size;
   unsigned int num = (size % BLOCK_SIZE == 0) ?
      size / BLOCK_SIZE : size / BLOCK_SIZE + 1;
   if (num <= NDIRECT) {
     for (unsigned int i = 0; i < num; i++) {
       bm->free_block(ino->blocks[i]);
     }
   }
   else {
     for (unsigned int i = 0; i < NDIRECT; i++) {
       bm->free_block(ino->blocks[i]);
     }
     bm->read_block(ino->blocks[NDIRECT], buf);
     blockid_t *blocks = (blockid_t *) buf;
     for (unsigned int i = 0; i < num - NDIRECT; i++) {
       bm->free_block(blocks[i]);
     }
     bm->free_block(ino->blocks[NDIRECT]);
   }

   // free the inode;
   free_inode(inum);
   free(ino);
   return;
}

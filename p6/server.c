#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "udp.h"
#include "mfs.h"

int debug = 1;

#define MAX_NUM_FILES (4096)
#define NUM_BITS_IN_CHAR (sizeof(char) * 8)
// number of bits in one char
#define BITMAP_REGION_SIZE (512)
// 4096 bits = 512 bytes = 512 chars
#define NUM_ENTS_IN_BLK (MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t))
#define TYPE_INIT (0)
#define TYPE_LOOKUP (1)
#define TYPE_STAT (2)
#define TYPE_WRITE (3)
#define TYPE_READ (4)
#define TYPE_CREAT (5)
#define TYPE_UNLINK (6)

typedef struct __instr_t {
  int type; // instrution type
  int inum; // inum or pinum, param 1 of everything;
  int creatType; // param 2 of MFS_Creat();
  char name[PATH_MAX]; // param 2 of MFS_Lookup(), MFS_Unlink()
              // and param 3 of MFS_Creat()
  char buffer[PATH_MAX]; // param 2 of MFS_Write() and MFS_Read();
  int block; // param 3 of MFS_Write() and MFS_Read();
  MFS_Stat_t stat; // param 2 of MFS_Stat();
  int rV; // return value of server;
} instr_t;

typedef struct __i_node_t {
  //  int magic; // to validate iNode corruption
  MFS_Stat_t fileStat; // contains file stats
  int dBlkPtr[10]; // 10 direct data block pointers, -1 means not used
} iNode_t;

/*******************
 * global variables
 *******************/
/** important vars **/
int fd;
unsigned char bmInit = 0x0;
unsigned char cmInit = 0x1 << (NUM_BITS_IN_CHAR - 1);
//int enabled = 1;
char dBinit = '\0';
/** size of each region, -1 for initial value **/
int ibmSize = BITMAP_REGION_SIZE;//-1; // iNode bitmap region size
int dbmSize = BITMAP_REGION_SIZE;//-1; // data block bitmap size
int inSize = MAX_NUM_FILES * sizeof(iNode_t);//-1; // iNode region size
int dbSize = MAX_NUM_FILES * MFS_BLOCK_SIZE;//-1; // data block size
/** other helper vars **/
int readBytes = 0; // debug helper var, will appear in every read/write call
char rootName[] = ".";
char parentName[] = "..";



/*******************
 * helper functions
 *******************/

/**
 * Initialize file system image
 * @param fd: file descriptor
 * @return 0 if success, -1 if failed
 */
int Image_Init(int fd){
  int i = 1;
  if(fd <= 0){
    return -1;
  }
  /** calculate statues variables **/
  ibmSize = BITMAP_REGION_SIZE;
  dbmSize = BITMAP_REGION_SIZE;
  inSize = MAX_NUM_FILES * sizeof(iNode_t);
  dbSize = MAX_NUM_FILES * MFS_BLOCK_SIZE;
  /** construct a parentNode **/
  iNode_t* pParentNode = (iNode_t*) malloc(sizeof(iNode_t));
  pParentNode->fileStat.type = MFS_DIRECTORY;
  pParentNode->fileStat.size = 2 * sizeof(MFS_DirEnt_t);// 0;
  pParentNode->fileStat.blocks = 1;
  pParentNode->dBlkPtr[0] = 0;
  for(i = 1; i < 10; i++){
    pParentNode->dBlkPtr[i] = -1;
  }
  /** construct . and .. entries of parentNode **/
  //char* initBlock = (char*)malloc(MFS_BLOCK_SIZE);
  MFS_DirEnt_t* entries = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);
  entries[0].inum = 0;
  entries[1].inum = 0;
  for(i = 2; i < NUM_ENTS_IN_BLK; i++){
    entries[i].inum = -1;
  }
  strcpy(entries[0].name, rootName);
  strcpy(entries[1].name, parentName);
  /** write iNode bitmap region **/
  lseek(fd, 0, SEEK_SET);
  /* readBytes = write(fd, &enabled, sizeof(int)); */
  /* for(i = 1; i < MAX_NUM_FILES; i++){ */
  /*   readBytes = write(fd, &disabled, sizeof(int)); */
  /* } */
  //char bmInit2 = 0x1 << 31; // this makes a (1000..0)b
  readBytes = write(fd, &cmInit, sizeof(char));
  for(i = 1; i < BITMAP_REGION_SIZE; i++){
    readBytes = write(fd, &bmInit, sizeof(char));
  }
  /** write data bitmap region **/
  lseek(fd, ibmSize, SEEK_SET);
  /* readBytes = write(fd, &enabled, sizeof(int)); */
  /* for(i = 1; i < MAX_NUM_FILES; i++){ */
  /*   readBytes = write(fd, &disabled, sizeof(int)); */
  /* } */
  readBytes = write(fd, &cmInit, sizeof(char));
  for(i = 1; i < BITMAP_REGION_SIZE; i++){
    readBytes = write(fd, &bmInit, sizeof(char));
  }
  /** write parent iNode **/
  lseek(fd, ibmSize + dbmSize, SEEK_SET);
  readBytes = write(fd, pParentNode, sizeof(iNode_t));
  //fsync(fd);
  char* nullNode = (char*) malloc(sizeof(iNode_t));
  for(i = 1; i < MAX_NUM_FILES; i++){
    readBytes = write(fd, nullNode, sizeof(iNode_t)); 
  }
  lseek(fd, ibmSize + dbmSize + inSize, SEEK_SET);
  /** write data blocks **/
  readBytes = write(fd, entries, MFS_BLOCK_SIZE);
  char* nullBlock = (char*) malloc(MFS_BLOCK_SIZE);
  for(i = 1; i < MAX_NUM_FILES; i++){
    readBytes = write(fd, nullBlock, MFS_BLOCK_SIZE);
  }
  if(debug == 1){
    printf("iNode bitmap size: %d bytes\n", ibmSize);
    printf("data bitmap size: %d bytes\n", dbmSize);
    printf("iNode region size: %d bytes\n", inSize);
    printf("data block region size: %d bytes\n", dbSize);
  }
  //free(initBlock);
  free(nullBlock);
  free(nullNode);
  fsync(fd);
  return 0;
}


/**
 * akes the parent inode number and looks up the entry name in it
 * @param pinum: parent iNode number
 * @param name: name of the file
 * @return the iNode number of file "name", -1 if failed
 */
int Image_Lookup(int pinum, char* name){
  if(pinum < 0 || pinum >= MAX_NUM_FILES){
    return -1;
  }
  unsigned char valid;
  /** check if pinum is valid **/
  /* lseek(fd, pinum, SEEK_SET); */
  /* readBytes = read(fd, &valid, sizeof(int)); */
  /* if(valid == 0){ */
  /*   return -1; */
  /* } */
  int offset1 = pinum / NUM_BITS_IN_CHAR;
  int offset2 = pinum % NUM_BITS_IN_CHAR;
  lseek(fd, offset1, SEEK_SET);
  readBytes = read(fd, &valid, sizeof(int));
  unsigned char comparator = cmInit >> (offset2);// - 1);
  if((valid & comparator) == 0){
    // this bit is not valid
    return -1;
  }
  /** locate the iNode of parent dir **/
  lseek(fd, (ibmSize + dbmSize + pinum * sizeof(iNode_t)), SEEK_SET);
  iNode_t* pParentNode = (iNode_t*) malloc(sizeof(iNode_t));
  readBytes = read(fd, pParentNode, sizeof(iNode_t));
  // check validity of this node 
  /* if(pParentNode->magic != MAGIC){ */
  /*   return -1; */
  /* } */
  if(!pParentNode->fileStat.type == MFS_DIRECTORY){
    return -1;
  }
  // start to look up file
  int i = 0;
  for(i = 0; i < pParentNode->fileStat.blocks; i++){
    int blockPointer = pParentNode->dBlkPtr[i];
    if(blockPointer != -1){
      // char* pCurrBlock = (char*) malloc(MFS_BLOCK_SIZE);
      MFS_DirEnt_t* entries = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);
      // locate current block
      lseek(fd, ibmSize + dbmSize + inSize + blockPointer * MFS_BLOCK_SIZE, SEEK_SET);
      // read in this block
      readBytes = read(fd, entries, MFS_BLOCK_SIZE);
      // check current block for a file entry named "name"
      //MFS_DirEnt_t* entries = (void*)pCurrBlock;
      int j;
      for(j = 0; j < NUM_ENTS_IN_BLK; j++){
	if(strcmp(entries[j].name, name) == 0){
	  // found match
	  return entries[j].inum;
	}
      }
    }
  }
  // if control reached here, no match is found
  return -1;
}

/**
 * returns information about the file specified by inum.
 * @param inum: the inum
 * @param m: stat struct
 * @return 0 if succeeded, -1 if failed
 */
int Image_Stat(int inum, MFS_Stat_t *m){
  if(inum < 0 || inum >= MAX_NUM_FILES){
    return -1;
  }
  // check if iNode is valid
  unsigned char valid;
  /* lseek(fd, inum, SEEK_SET); */
  /* readBytes = read(fd, &valid, sizeof(int)); */
  /* if(valid == 0){ */
  /*   return -1; */
  /* } */
  int offset1 = inum / NUM_BITS_IN_CHAR;
  int offset2 = inum % NUM_BITS_IN_CHAR;
  lseek(fd, offset1, SEEK_SET);
  readBytes = read(fd, &valid, sizeof(char));
  unsigned char comparator = cmInit >> (offset2);// - 1);
  if((valid & comparator) == 0){
    // inum not valid
    return -1;
  }
  //printf("I node is existed = %d\n",valid & comparator);


  // locate the inode
  lseek(fd, (ibmSize + dbmSize + inum * sizeof(iNode_t)), SEEK_SET);
  iNode_t* pThisNode = (iNode_t*) malloc(sizeof(iNode_t));
  readBytes = read(fd, pThisNode, sizeof(iNode_t));
  /* if(pThisNode->magic != MAGIC){ */
  /*   return -1; */
  /* } */
  m->type = pThisNode->fileStat.type;
  m->size = pThisNode->fileStat.size;
  m->blocks = pThisNode->fileStat.blocks;
  return 0;
}

/**
 * writes a block of size 4096 bytes at the block offset specified by "block"
 * @param inum: the inum
 * @param buffer: data to be written
 * @param block: the block offset to be written  
 * @return 0 if succeeded, -1 if failed
 */
int Image_Write(int inum, char *buffer, int block){
  if(inum < 0 || inum >= MAX_NUM_FILES){
    return -1;
  }
  /** check if iNode is valid **/
  unsigned char valid;
  /* lseek(fd, inum * sizeof(int), SEEK_SET); */
  /* readBytes = read(fd, &valid, sizeof(int)); */
  /* if(valid == 0){ */
  /*   return -1; */
  /* } */
  int offset1 = inum / NUM_BITS_IN_CHAR;
  int offset2 = inum % NUM_BITS_IN_CHAR;
  lseek(fd, offset1, SEEK_SET);
  readBytes = read(fd, &valid, sizeof(char));
  unsigned char comparator = cmInit >> (offset2);// - 1);
  if((valid & comparator) == 0){
    // inum is not valid
    return -1;
  }
  /** get the iNode **/
  lseek(fd, (ibmSize + dbmSize + inum * sizeof(iNode_t)), SEEK_SET);
  iNode_t* pThisNode = (iNode_t*) malloc(sizeof(iNode_t));
  readBytes = read(fd, pThisNode, sizeof(iNode_t));
  /* if(pThisNode->magic != MAGIC){ */
  /*   return -1; */
  /* } */
  if(pThisNode->fileStat.type == MFS_DIRECTORY){
    // this is a directory, you can't write to a directory
    return -1;
  }
  if(block > pThisNode->fileStat.blocks){
    // write to invalid block
    return -1;
  }
  /** get block pointer **/
  int blockPtr = pThisNode->dBlkPtr[block];
  if(blockPtr == -1){
    // create a new block
    // traverse data bitmap to find an empty block
    unsigned char valid;
    int itr = 0;
    int offset1 = 0;
    int offset2 = -1;
    int found = 0;
    lseek(fd, ibmSize, SEEK_SET);
    while(found == 0 && itr < BITMAP_REGION_SIZE){
      readBytes = read(fd, &valid, sizeof(int));
      int i = 0;
      for(i = 0; i < NUM_BITS_IN_CHAR && found == 0; i++){
	unsigned char comparator = cmInit >> i;
	if((valid & comparator) == 0){
	  // found an empty block
	  found = 1;
	  offset2 = i;
	}
      }
      itr++;
    }
    if(found == 1){
      offset1 = itr - 1;
      blockPtr = offset1 * NUM_BITS_IN_CHAR + offset2;
    } else {
      return -1;
    }
    // update data block pointer in iNode
    pThisNode->dBlkPtr[block] = blockPtr;
    pThisNode->fileStat.blocks++;
    pThisNode->fileStat.size += MFS_BLOCK_SIZE;
    // write data block 
    lseek(fd, (ibmSize + dbmSize + inSize + blockPtr * MFS_BLOCK_SIZE), SEEK_SET);
    readBytes = write(fd, buffer, MFS_BLOCK_SIZE);
    // write updated iNode back
    lseek(fd, ibmSize + dbmSize + inum * sizeof(iNode_t), SEEK_SET);
    readBytes = write(fd, pThisNode, sizeof(iNode_t));
    // update data bitmap
    //char valid;
    offset1 = blockPtr / NUM_BITS_IN_CHAR;
    offset2 = blockPtr % NUM_BITS_IN_CHAR; 
    lseek(fd, (ibmSize + offset1), SEEK_SET);
    readBytes = read(fd, &valid, sizeof(int));
    unsigned char comparator = cmInit >> (offset2);// - 1);
    valid = valid | comparator;
    lseek(fd, (ibmSize + offset1), SEEK_SET);
    readBytes = write(fd, &valid, sizeof(char));
  } else {
    // write to an existing block
    // locate this block
    lseek(fd, (ibmSize + dbmSize + inSize + blockPtr * MFS_BLOCK_SIZE), SEEK_SET);
    // write data into it    
    readBytes = write(fd, &buffer, MFS_BLOCK_SIZE);
    // update fileStat of this node
    pThisNode->fileStat.size += MFS_BLOCK_SIZE;
    // write iNode back
    lseek(fd, ibmSize + dbmSize + inum * sizeof(iNode_t), SEEK_SET);
    readBytes = write(fd, pThisNode, sizeof(iNode_t));
    // no other things need update, so that's it
  }
  fsync(fd);
  return 0;
}

/**
 * reads a block specified by block into the buffer from file specified by inum
 * @param inum: iNode number
 * @param buffer: buffer to store data
 * @param block: block offset
 * @return 0 if succeeded, -1 if failed
 */
int Image_Read(int inum, char* buffer, int block){
  if(inum < 0 || inum >= MAX_NUM_FILES){
    return -1;
  }
  /** check if iNode is valid **/
  unsigned char valid = 0;
  /* lseek(fd, inum, SEEK_SET); */
  /* readBytes = read(fd, &valid, sizeof(int)); */
  /* if(valid == 0){ */
  /*   return -1; */
  /* } */
  int offset1 = inum / NUM_BITS_IN_CHAR;
  int offset2 = inum % NUM_BITS_IN_CHAR;
  lseek(fd, offset1, SEEK_SET);
  readBytes = read(fd, &valid, sizeof(char));
  unsigned char comparator = cmInit >> (offset2);// - 1);
  if((valid & comparator) == 0){
    // inum is not valid
    return -1;
  }
  /** get the iNode **/
  lseek(fd, (ibmSize + dbmSize + inum * sizeof(iNode_t)), SEEK_SET);
  iNode_t* pThisNode = (iNode_t*) malloc(sizeof(iNode_t));
  readBytes = read(fd, pThisNode, sizeof(iNode_t));
  /* if(pThisNode->magic != MAGIC){ */
  /*   return -1; */
  /* } */
  // check if block enrty is valid
  if(pThisNode->fileStat.blocks <= block){
    return -1;
  }
  int blockPtr = pThisNode->dBlkPtr[block];
  if(blockPtr == -1){
    return -1;
  }
  // get data block
  //  char* buffer = (char*) malloc(MFS_BLOCK_SIZE);
  lseek(fd, (ibmSize + dbmSize + inSize + blockPtr * MFS_BLOCK_SIZE), SEEK_SET);
  readBytes = read(fd, buffer, MFS_BLOCK_SIZE);
  fsync(fd);
  return 0;
}


/**
 * makes a file or directory in the parent directory
 * @param pinum: parent iNode
 * @param type: indicates file/dir
 * @param name: the name of new file
 * @return 0 if succeeded, -1 if failed
 * note: creating an file with existing name is considered as success
 * this method is fxxking complicated. Initiate all variables up front
 */
int Image_Creat(int pinum, int type, char* name){
  /** initiate all variables needed **/
  unsigned char* ibm = (unsigned char*)malloc(BITMAP_REGION_SIZE);
  unsigned char* dbm = (unsigned char*)malloc(BITMAP_REGION_SIZE);
  int inum = -1; // iNode number for newly created Dir/File
  iNode_t* pParentNode = (iNode_t*) malloc(sizeof(iNode_t)); // iNode of parent dir
  iNode_t* pThisNode = (iNode_t*) malloc(sizeof(iNode_t)); // iNode of newly created dir/file
  int blockOffset = -1; // the block offset where empty entry is located(or when creating a new block in parent dir)
  int entryOffset = -1; // the entry inside a block where empty entry is lovated(when creating a new block, this is 0)
  int createNewMode = 0; // indicates whether put into existing empty entry(1) or make a new block(0)
  MFS_DirEnt_t* thisBlock; // the block newly created to put a new entry(not initialized)
  MFS_DirEnt_t* newEntryBlock; // the block where existing empty entry was located(not initialized)
  int blockOffsetNew = -1; // the block offset of newly created dir
  MFS_DirEnt_t* newBlock; // the new block of newly created dir where . and .. entry should be initialized(not initialized)
  /** read in bitmaps **/
  lseek(fd, 0, SEEK_SET);
  readBytes = read(fd, ibm, BITMAP_REGION_SIZE);
  readBytes = read(fd, dbm, BITMAP_REGION_SIZE);
  /** input param validity check **/
  if(pinum < 0 || pinum >= MAX_NUM_FILES){
    return -1;
  }
  unsigned char valid;
  /* lseek(fd, pinum, SEEK_SET); */
  /* readBytes = read(fd, &valid, sizeof(int)); */
  /* if(valid == 0){ */
  /*   return -1;   */
  /* } */
  int offset1 = pinum / NUM_BITS_IN_CHAR;
  int offset2 = pinum % NUM_BITS_IN_CHAR;
  /* lseek(fd, offset1, SEEK_SET); */
  /* readBytes = read(fd, &valid, sizeof(char)); */
  valid = ibm[offset1];
  unsigned char comparator = cmInit >> (offset2);// - 1);
  if((valid & comparator) == 0){
    // pinum is not valid
    return -1;
  }
  /** get parent node and check validty **/
  lseek(fd, (ibmSize + dbmSize + pinum * sizeof(iNode_t)), SEEK_SET);
  readBytes = read(fd, pParentNode, sizeof(iNode_t));
  if(pParentNode->fileStat.type == MFS_REGULAR_FILE){
    return -1;
  }
  /** check if name already exists in this dir **/
  int i = 0;
  int found = 0; // indicates if an existing -1 entry is avaliable
  for(i = 0; i< pParentNode->fileStat.blocks; i++){
    int blockPointer = pParentNode->dBlkPtr[i];
    if(blockPointer != -1){
      // this is a valid block pointer, get this block
      lseek(fd, (ibmSize + dbmSize + inSize + blockPointer * MFS_BLOCK_SIZE), SEEK_SET);
      //char* buffer = (char*) malloc(MFS_BLOCK_SIZE);
      MFS_DirEnt_t* entries = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);      
      readBytes = read(fd, entries, MFS_BLOCK_SIZE);
      int k = 0;
      for(k = 0; k < NUM_ENTS_IN_BLK; k++){
	if(entries[k].inum == -1 && found == 0){
	  // do some work for finding empty block...
	  createNewMode = 1; // set create mode to "existng entry"(1)
	  blockOffset = blockPointer; // log blockOffset as current block pointer
	  entryOffset = k; // log entry offset as current k
	  newEntryBlock = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);
	  /* char* itr = entries; */
	  /* char* tmp = newEntryBlock; */
	  /* while(itr != NULL){ */
	  /*   *tmp = *itr; */
	  /*   tmp++; */
	  /*   itr++; */
	  /* } // copy this block which contains an empty block to newEntryBlock */
	  /* // TODO: not sure if this is correct, test later */
	  // copy this block which contains an empty block to newEntryBlock
	  memcpy(newEntryBlock, entries, MFS_BLOCK_SIZE);
	  found = 1;
	} else {
	  // this is not an empty entry
	  if(strcmp(entries[k].name, name) == 0){
	    // file specified by "name" already exists, return success. No modification is made
	    return 0;
	  }
	}
      }
    }
  }
  // if out of this loop, means that name does not exist. Let's create it!
  /** make a new iNode for this new file or dir **/
  //lseek(fd, 0, SEEK_SET);
  found = 0;
  int itr = 0;
  //valid;
  offset1 = 0;
  offset2 = -1;
  while(found == 0 && itr < BITMAP_REGION_SIZE){
    //readBytes = read(fd, &valid, sizeof(int));
    valid = ibm[offset1];    
    int i = 0;
    for(i = 0; i < NUM_BITS_IN_CHAR && found == 0; i++){
      unsigned char comparator = cmInit >> i;
      if((valid & comparator) == 0){
	found = 1;
	offset2 = i;
      }
    }
    offset1++;
  }
  if(found == 0){
    // no empty inum found
    return -1;
  }
  inum = (offset1 - 1) * NUM_BITS_IN_CHAR + offset2;
  comparator = cmInit >> offset2;
  ibm[offset1 - 1] = ibm[offset1 - 1] | comparator;
  //pThisNode->magic = MAGIC;
  pThisNode->fileStat.type = type;
  if(type == MFS_DIRECTORY){
    pThisNode->fileStat.size = 2 * sizeof(MFS_DirEnt_t);
  } else {
    pThisNode->fileStat.size = 0;
  }
  //if(type == MFS_DIRECTORY){
  //pThisNode->fileStat.blocks = 1;
  //} else {
  pThisNode->fileStat.blocks = 0;
  //} 
  i = 0;
  for (i = 0; i < 10; i++){
    pThisNode->dBlkPtr[i] = -1;
  }
  /** write this inum into parentNode **/
  if(createNewMode == 1){
    // write into existing parent dir block
    // blockOffset is already kept logged
    //MFS_DirEnt_t* entries = (void*)newEntryBlock;
    newEntryBlock[entryOffset].inum = inum;
    strcpy(newEntryBlock[entryOffset].name, name);
  } else {
    // write to a new block
    // check if we can still assign new blocks to parent dir
    if(pParentNode->fileStat.blocks >= 10){
      // no more blocks could be assigned
      return -1;
    }
    // find a new block
    /* lseek(fd, ibmSize, SEEK_SET); */
    /* int valid = 1; */
    /* int itr = 0; */
    /* while(valid != 0 && itr < MAX_NUM_FILES){ */
    /*   readBytes = read(fd, &valid, sizeof(int)); */
    /*   itr++; */
    /* } */
    /* if(valid != 0){ */
    /*   // no avaliable inum to be assigned */
    /*   return -1; */
    /* } */
    /* blockOffset = itr - 1; */
    int found = 0;
    int offset1 = 0;
    int offset2 = -1;
    int itr = 0;
    //lseek(fd, ibmSize, SEEK_SET);
    while(found == 0 && itr < BITMAP_REGION_SIZE){
      //char vaild;      
      //readBytes = read(fd, &valid, sizeof(char));
      valid = dbm[itr];
      int i = 0;
      for(i = 0; i < NUM_BITS_IN_CHAR && found == 0; i++){
	unsigned char comparator = cmInit >> i;
	if((valid & comparator) == 0){
	  found = 1;
	  offset2 = i;
	}
      }
      itr++;
    }
    if(found == 0){
      return -1;
    }
    offset1 = itr - 1;
    blockOffset = offset1 * NUM_BITS_IN_CHAR + offset2;
    unsigned char comparator = cmInit >> offset2;
    dbm[offset1] = dbm[offset1] | comparator;
    // get this new block
    /* char buffer[MFS_BLOCK_SIZE]; */

    thisBlock = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);
    thisBlock[0].inum = inum;
    strcpy(thisBlock[0].name, name);
    int i = 0;
    for(i = 1; i < NUM_ENTS_IN_BLK; i++){
      thisBlock[i].inum = -1;
      //strcpy(entries[i].name, &dBInit);
    }
  }
  // now parentNode is ready, new iNode is ready. Are we done here?
  // unfortunately, no.(Shiiiiiiiiiiiiit!!!!) If this new "name" is a directory, we have to initialize its . and .. entries
  /** initialize new directory **/
  if(type == MFS_DIRECTORY){
    // find an avaliable data block
    /* lseek(fd, ibmSize, SEEK_SET); */
    /* int valid = 1; */
    /* int itr = 0; */
    /* while(valid != 0 && itr < MAX_NUM_FILES){ */
    /*   readBytes = read(fd, &valid, sizeof(int)); */
    /*   itr++; */
    /* } */
    /* if(valid != 0){ */
    /*   // no avaliable data blocks */
    /*   return -1; */
    /* } */
    int found = 0;
    int offset1 = 0;
    int offset2 = 0;
    int itr = 0;
    char valid;
    //lseek(fd, ibmSize, SEEK_SET);
    while(found == 0 && itr < BITMAP_REGION_SIZE){
      //readBytes = read(fd, &valid, sizeof(char));
      valid = dbm[itr];
      int i = 0;
      for(i = 0; i < NUM_BITS_IN_CHAR && found == 0; i++){
	unsigned char comparator = cmInit >> i;
	if((valid & comparator) == 0){
	  found = 1;
	  offset2 = i;
	}
      }
      itr++;
      offset1 = itr - 1;
    }
    if(found == 0){
      return -1;
    }
    blockOffsetNew = offset1 * NUM_BITS_IN_CHAR + offset2;
    unsigned char comparator = cmInit >> offset2;
    dbm[offset1] = dbm[offset1] | comparator;
    // update this node
    pThisNode->dBlkPtr[0] = blockOffsetNew;
    pThisNode->fileStat.blocks++;
    // initialize this block
    //newBlock = (char*) malloc(MFS_BLOCK_SIZE);
    //MFS_DirEnt_t* entries = (void*)newBlock;
    newBlock = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);    
    newBlock[0].inum = 0;
    strcpy(newBlock[0].name, ".");
    newBlock[1].inum = pinum;
    strcpy(newBlock[1].name, "..");
    int i = 0;
    for(i = 2; i < NUM_ENTS_IN_BLK; i++){
      newBlock[i].inum = -1;
    }
    // update thisNode
    //pThisNode->dBlkPtr[pThisNode->fileStat.blocks] = blockOffsetNew;
    //pThisNode->fileStat.blocks++;
  }
  // all jobs done.
  // note that sofar no change has done on our actual file
  // so let's write all changes back to file
  /** write back phase **/
  // newBlock: blockOffsetNew, if needed
  if(type == MFS_DIRECTORY){
    lseek(fd, (ibmSize + dbmSize + inSize + blockOffsetNew * MFS_BLOCK_SIZE), SEEK_SET);
    readBytes = write(fd, newBlock, MFS_BLOCK_SIZE);
  }
  // newEntryBlock: blockOffset (existing, mode 1) or thisBlock: blockOffset (new, mode 0)
  if(createNewMode == 1){
    pParentNode->fileStat.size += sizeof(MFS_DirEnt_t);
    lseek(fd, (ibmSize + dbmSize + inSize + blockOffset * MFS_BLOCK_SIZE), SEEK_SET);
    readBytes = write(fd, newEntryBlock, MFS_BLOCK_SIZE);
  } else {
    pParentNode->fileStat.size += sizeof(MFS_DirEnt_t);
    pParentNode->dBlkPtr[pParentNode->fileStat.blocks] = blockOffset;
    pParentNode->fileStat.blocks++;
    lseek(fd, (ibmSize + dbmSize + inSize + blockOffset * MFS_BLOCK_SIZE), SEEK_SET);
    readBytes = write(fd, thisBlock, MFS_BLOCK_SIZE);
  }
  // pThisNode, inum
  lseek(fd, (ibmSize + dbmSize + inum * sizeof(iNode_t)), SEEK_SET);
  readBytes = write(fd, pThisNode, sizeof(iNode_t));
  // pParentNode, pinum
  lseek(fd, (ibmSize + dbmSize + pinum * sizeof(iNode_t)), SEEK_SET);
  readBytes = write(fd, pParentNode, sizeof(iNode_t));

  lseek(fd, 0, SEEK_SET);
  readBytes = write(fd, ibm, BITMAP_REGION_SIZE);
  readBytes = write(fd, dbm, BITMAP_REGION_SIZE);
  fsync(fd);
  return 0;
} 


/**
 * removes the file or directory name from the directory specified by pinum
 * @param pinum: the parent directory iNode number
 * @param name: file/directory to be unlinked
 * @return 0 if succeeded, -1 if failed
 */
int Image_Unlink(int pinum, char* name){
  /** check input validity **/
  if(pinum < 0 || pinum >= MAX_NUM_FILES){
    return -1;
  }
  /** initialize some variables **/
  iNode_t* pParentNode = (iNode_t*) malloc(sizeof(iNode_t));
  iNode_t* pThisNode = (iNode_t*) malloc(sizeof(iNode_t));
  int DBPtrs[10]; // the data block pointers of "name"
  int inum = -1; // the inum of "name"1
  int blockOffset = -1; // the block pointer of parent dir block containing "name"
  int entryOffset = -1; // the netry offset in parent node
  MFS_DirEnt_t* parentBlock; // the parent block which contains "name" entry
  /** check parentNode validity **/
  /* lseek(fd, pinum, SEEK_SET); */
  /* int valid = 0; */
  /* readBytes = read(fd, &valid, sizeof(int)); */
  /* if(valid == 0){ */
  /*   // pinum is invalid */
  /*   return -1; */
  /* } */
  unsigned char valid;
  int offset1 = pinum / NUM_BITS_IN_CHAR;
  int offset2 = pinum % NUM_BITS_IN_CHAR;
  lseek(fd, offset1, SEEK_SET);
  readBytes = read(fd, &valid, sizeof(char));
  unsigned char comparator = cmInit >> (offset2);// - 1);
  if((valid & comparator) == 0){
    // pinum is not valid
    return -1;
  }
  /** get parentNode **/
  lseek(fd, (ibmSize + dbmSize + pinum * sizeof(iNode_t)), SEEK_SET);
  readBytes = read(fd, pParentNode, sizeof(iNode_t));
  if(pParentNode->fileStat.type != MFS_DIRECTORY){
    // parent node incompatible
    return -1;
  }
  /** search for "name" in parentNode **/
  int i = 0;
  for(i = 0; i< pParentNode->fileStat.blocks; i++){
    int blockPointer = pParentNode->dBlkPtr[i];
    if(blockPointer != -1){
      // this is a valid block pointer, get this block
      //char* buffer = (char*) malloc(MFS_BLOCK_SIZE);
      MFS_DirEnt_t* entries = (MFS_DirEnt_t*)malloc(MFS_BLOCK_SIZE);
      lseek(fd, (ibmSize + dbmSize + inSize + blockPointer * MFS_BLOCK_SIZE), SEEK_SET);
      readBytes = read(fd, entries, MFS_BLOCK_SIZE);
      //MFS_DirEnt_t* entries = (void*)buffer;
      int k = 0;
      for(k = 0; k < NUM_ENTS_IN_BLK; k++){
	if(entries[k].inum != -1 && strcmp(entries[k].name, name) == 0){
	  // this is the entry of "name"
	  // save the information of this entry
	  inum = entries[k].inum;
	  blockOffset = blockPointer;
	  entryOffset = k;
	  //parentBlock = (char*) malloc(MFS_BLOCK_SIZE);
	  parentBlock = (MFS_DirEnt_t*)malloc(MFS_BLOCK_SIZE);
	  /* char* itr = entries; */
	  /* char* tmp = parentBlock; */
	  /* // copy this whole block to modify it */
	  /* while(itr != NULL){ */
	  /*   *tmp = *itr; */
	  /*   tmp++; */
	  /*   itr++; */
	  /* }// TODO: not sure if this is correct, test later */
	  // copy this whole block
	  memcpy(parentBlock, entries, MFS_BLOCK_SIZE);
	}
      }
    }
  }
  // if out of this loop, check found
  if(inum == -1){
    // means that "name" does not exist in this dir
    return 0;
  }
  /** get the inode of "name" **/
  lseek(fd, (ibmSize + dbmSize + inum * sizeof(iNode_t)), SEEK_SET);
  readBytes = read(fd, pThisNode, sizeof(iNode_t));
  if(pThisNode->fileStat.type == MFS_DIRECTORY){
    // this is a directory, check if it is empty
    int i = 0;
    for(i = 0; i < 10; i++){
      int blockPointer = pThisNode->dBlkPtr[i];
      DBPtrs[i] = blockPointer; // do a little work for later deleting job
      if(blockPointer != -1){
	// get this block
	//char* buffer = (char*) malloc(MFS_BLOCK_SIZE);
	MFS_DirEnt_t* entries = (MFS_DirEnt_t*) malloc(MFS_BLOCK_SIZE);
	lseek(fd, (ibmSize + dbmSize + inSize + blockPointer * MFS_BLOCK_SIZE), SEEK_SET);
	readBytes = read(fd, entries, MFS_BLOCK_SIZE);
	//MFS_DirEnt_t* entries = (void*)buffer;
	int k = 0;
	for(k = 0; k < NUM_ENTS_IN_BLK; k++){
	  if(entries[k].inum != -1){
	    // check if it's . or ..
	    if(strcmp(entries[k].name, ".") != 0 && strcmp(entries[k].name, "..") != 0){
	      // this directory is not empty
	      return -1;
	    }
	  }	    
	}
      }
    }
  } else {
    // this is a regular file, just delete it 
    int i = 0;
    for(i = 0; i < 10; i++){
      DBPtrs[i] = pThisNode->dBlkPtr[i];
    }
  }
  if(parentBlock != NULL){
    /** update entries in parent node **/
    //MFS_DirEnt_t* entries = (void*)parentBlock;
    parentBlock[entryOffset].inum = -1;
    pParentNode->fileStat.size -= sizeof(MFS_DirEnt_t);
    /** write thisblock back to file **/
    lseek(fd, (ibmSize + dbmSize + inSize + blockOffset * MFS_BLOCK_SIZE), SEEK_SET);
    readBytes = write(fd, parentBlock, MFS_BLOCK_SIZE);
    /** write parentNode back to file **/
    lseek(fd, (ibmSize + dbmSize + pinum * sizeof(iNode_t)), SEEK_SET);
    readBytes = write(fd, pParentNode, sizeof(iNode_t));
    /** update iNode bitmap and data bitmap **/
    /* lseek(fd, inum, SEEK_SET); */
    /* readBytes = write(fd, &disabled, sizeof(int)); */
    /* int i = 0;  */
    /* for(i = 0; i < 10; i++){ */
    /*   if(DBPtrs[i] != -1){ */
    /* 	lseek(fd, (ibmSize + DBPtrs[i]), SEEK_SET); */
    /* 	readBytes = write(fd, &disabled, sizeof(int)); */
    /*   } */
    /* } */
    unsigned char valid;
    int offset1 = inum / NUM_BITS_IN_CHAR;
    int offset2 = inum % NUM_BITS_IN_CHAR;
    lseek(fd, offset1, SEEK_SET);
    readBytes = read(fd, &valid, sizeof(char));
    unsigned char comparator = cmInit >> (offset2);// - 1);
    //printf("I node is existed = %d\n",valid & comparator);
    valid = valid ^ comparator;

    //printf("I node is existed = %d\n",valid & comparator);

    lseek(fd, offset1, SEEK_SET);
    readBytes = write(fd, &valid, sizeof(char));
    int i = 0;
    for(i = 0; i < 10; i++){
      int thisPtr = DBPtrs[i];
      if(thisPtr != -1){
	//char valid;
	int offset1 = thisPtr / NUM_BITS_IN_CHAR;
	int offset2 = thisPtr % NUM_BITS_IN_CHAR;
	lseek(fd, ibmSize + offset1, SEEK_SET);
	readBytes = read(fd, &valid, sizeof(char));
	unsigned char comparator = cmInit >> (offset2);// - 1);
	valid = valid | comparator;
	lseek(fd, ibmSize + offset1, SEEK_SET);
	readBytes = write(fd, &valid, sizeof(char));
      }
    }
    fsync(fd);
    return 0;
  } else {
    return -1;
  }
}


/***************************
 * here comes the main func
 ***************************/
int
main(int argc, char *argv[])
{

  if(argc != 3)
    {
      printf("Usage: server [port-num] [file-system-image]\n");
      exit(1);
    }

  int portid = atoi(argv[1]);
  char imgFileName[PATH_MAX];
  strcpy(imgFileName, argv[2]);
  int sd = UDP_Open(portid); //port # 
  assert(sd > -1);
    
  /** check if file system image file exists **/
  int fd_t = open(imgFileName, O_RDWR);
  fd = fd_t;
  if(fd_t < 0){
    // file not exist. create one.
    fd = open(imgFileName, O_RDWR | O_CREAT);
    fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    // initialize this file
    Image_Init(fd);
  }

  if(debug){
    printf("waiting in loop\n");
  }

  while (1) {
    struct sockaddr_in s;
    //char buffer[BUFFER_SIZE];
    instr_t* instr = (instr_t*) malloc(sizeof(instr_t));
    int rc = UDP_Read(sd, &s, (char*)instr, sizeof(instr_t)); //read message buffer from port sd
    int rV;
    int ifStat = 0; // indicates if this is a MFS_Stat() call
    if (rc > 0) {
      if(debug){
	printf("SERVER:: read %d bytes (instr type: '%d')\n", rc, instr->type);
      }
      switch(instr->type){
      case TYPE_LOOKUP:
	rV = Image_Lookup(instr->inum, instr->name);
	break;
      case TYPE_STAT:
	ifStat = 1;
	rV = Image_Stat(instr->inum, &(instr->stat));
	break;
      case TYPE_WRITE:
	rV = Image_Write(instr->inum, instr->buffer, instr->block);
	break;
      case TYPE_READ:
	ifStat = 1;
	rV = Image_Read(instr->inum, instr->buffer, instr->block);
	break;
      case TYPE_CREAT:
	rV = Image_Creat(instr->inum, instr->creatType, instr->name);
	break;
      case TYPE_UNLINK:
	rV = Image_Unlink(instr->inum, instr->name);
	break;
      }
      instr->rV = rV;
      if(debug){
	printf("type of request: %d. reply: %d\n", instr->type, rV);
      }
      if(ifStat){

	rc = UDP_Write(sd, &s, (char*)instr, sizeof(instr_t)); //write message buffer to port sd       
      } else {
	char* rVc = (char*) malloc(sizeof(int));
	//rVc = itoa(rV, rVc, 10);
	sprintf(rVc, "%d", rV);
	rc = UDP_Write(sd, &s, rVc, sizeof(int)); //write message buffer to port sd
      }
      //      sprintf(reply, "reply");
      //rc = UDP_Write(sd, &s, reply, BUFFER_SIZE); //write message buffer to port sd
    }
  }
  close(fd);
  return 0;
}



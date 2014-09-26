#include "mfs.h"
#include "udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define TYPE_INIT (0)
#define TYPE_LOOKUP (1)
#define TYPE_STAT (2)
#define TYPE_WRITE (3)
#define TYPE_READ (4)
#define TYPE_CREAT (5)
#define TYPE_UNLINK (6)


int debug = 0;

typedef struct __instr_t{
  int type; // instrution type
  int inum; // inum or pinum, param 1 of everything;
  int creatType; // param 2 of MFS_Creat();
  char name[PATH_MAX]; // param 2 of MFS_Lookup(), MFS_Unlink()
  // and param 3 of MFS_Creat()
  char buffer[BUFFER_SIZE]; // param 2 of MFS_Write() and MFS_Read();
  int block; // param 3 of MFS_Write() and MFS_Read();
  MFS_Stat_t stat; // param 2 of MFS_Stat();
  int rV; // return value of MFS_Stat();
} instr_t;

/** global variables **/
// networking related
//int portNo;
int fd;
int clientPort = 0;
//char hostName[_POSIX_HOST_NAME_MAX];
struct sockaddr_in addr;
struct sockaddr_in addr2;
int rc;
// file system related
// BUFFER_SIZE to instr_t


//char buffer[4096];

/** TimeOut Function **/
struct timeval timeOut;
fd_set sfd;



/** functions **/
/**
 * takes a host name and port number and uses those to find the server exporting the file system
 * @params hostname
 * @params port
 * @return 1 if succeeded, -1 if failed
 */
int MFS_Init(char *hostname, int port){
  //strcpy(hostName, hostname);
  fd = UDP_Open(clientPort); // establish communication
  //fd = UDP_Open(port); // establish communication
  if(fd < 0){
    return -1;
  }
  rc = UDP_FillSockAddr(&addr, hostname, port);
  if(rc != 0){
    return -1;
  }
  return 1;
}

/**
 * // TODO:
 */
int MFS_Lookup(int pinum, char *name){
  if(pinum < 0 || name == NULL){
    return -1;
  }

  int reSend;
  reSend=0;

  char buffer[sizeof(int)];
  instr_t* pInstr = (instr_t*)malloc(sizeof(instr_t));
  pInstr->type = TYPE_LOOKUP;
  // TODO: fill in pInstr
  pInstr->inum = pinum;
  strcpy(pInstr->name, name);
  //memcpy(pInstr->name, name,PATH_MAX);

  // initialize done;


  
  while(0==reSend)
    {
      rc = UDP_Write(fd, &addr, (char*)pInstr, sizeof(instr_t));
      if(rc <= 0){
	if(debug == 1){
	  printf("udp_write failed\n");
	}
	return -1;
      }
      FD_ZERO(&sfd);
      FD_SET(fd, &sfd);
        
      //wait for 5 seconds
      timeOut.tv_sec = 5;
      timeOut.tv_usec = 0;

      reSend = select(fd + 1, &sfd, NULL, NULL, &timeOut);

      if(1==reSend){
	rc = UDP_Read(fd, &addr2, buffer, sizeof(int));
	if(rc <= 0){
	  if(debug == 1){
	    printf("udp_write failed\n");
	  }
	  return -1;
	}
      }

    }

  int toBeReturned = atoi(buffer);
  return toBeReturned;
}

/**
 * // TODO:
 */
int MFS_Stat(int inum, MFS_Stat_t *m){
  if(inum < 0 || m == NULL){
    return -1;
  }

  int reSend;
  reSend=0;

  instr_t* pRInstr = (instr_t*)malloc(sizeof(instr_t));
  instr_t* pInstr = (instr_t*)malloc(sizeof(instr_t));
  pInstr->type = TYPE_STAT;
  // TODO: fill in pInstr
  pInstr->inum = inum;
  pInstr->stat.type = m->type;
  pInstr->stat.size = m->size;
  pInstr->stat.blocks = m->blocks;

  while(0==reSend){
    rc = UDP_Write(fd, &addr, (char*)pInstr, sizeof(instr_t));
    if(rc <= 0){
      if(debug == 1){
	printf("udp_write failed\n");
      }
      return -1;
    }

    FD_ZERO(&sfd);
    FD_SET(fd, &sfd);
        
    //wait for 5 seconds
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;

    reSend = select(fd + 1, &sfd, NULL, NULL, &timeOut);

    if(1==reSend){
    
      rc = UDP_Read(fd, &addr2, (char*)pRInstr, sizeof(instr_t));
      if(rc <= 0){
	if(debug == 1){
	  printf("udp_write failed\n");
	}
	return -1;
      }
    }
  }
  m->type = pRInstr->stat.type;
  m->size = pRInstr->stat.size;
  m->blocks = pRInstr->stat.blocks;
  int toBeReturned = pRInstr->rV;
  return toBeReturned;
}

int MFS_Write(int inum, char *buffer, int block){
  if(inum < 0 || buffer == NULL || block < 0){
    return -1;
  }

  int reSend;
  reSend=0;

  char rBuffer[sizeof(int)];
  instr_t* pInstr = (instr_t*)malloc(sizeof(instr_t));
  pInstr->type = TYPE_WRITE;
  // TODO: fill in pInstr
  pInstr->inum = inum;
  memcpy(pInstr->buffer, buffer,BUFFER_SIZE);
  //strcpy(pInstr->buffer, buffer);
  pInstr->block = block;


  while(0==reSend){
    rc = UDP_Write(fd, &addr, (char*)pInstr, sizeof(instr_t));
    if(rc <= 0){
      if(debug == 1){
	printf("udp_write failed\n");
      }
      return -1;
    }

    FD_ZERO(&sfd);
    FD_SET(fd, &sfd);
        
    //wait for 5 seconds
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;

    reSend = select(fd + 1, &sfd, NULL, NULL, &timeOut);


    if(1==reSend){
      rc = UDP_Read(fd, &addr2, rBuffer, sizeof(int));
      if(rc <= 0){
	if(debug == 1){
	  printf("udp_write failed\n");
	}
	return -1;
      }
    }
  }
  int toBeReturned = atoi(rBuffer);
  //  int toBeReturned = atoi(buffer);
  return toBeReturned;
}

int MFS_Read(int inum, char *buffer, int block){
  if(inum < 0 || buffer == NULL || block < 0){
    return -1;
  }

  int reSend;
  reSend=0;
  //  char rBuffer[sizeof(int)];
  instr_t* pInstr = (instr_t*)malloc(sizeof(instr_t));
  instr_t* pRInstr = (instr_t*)malloc(sizeof(instr_t));
  pInstr->type = TYPE_READ;
  // TODO: fill in pInstr
  pInstr->inum = inum;

  memcpy(pInstr->buffer, buffer,BUFFER_SIZE);

  //strcpy(pInstr->buffer, buffer);
  pInstr->block = block;

  while(0==reSend){

    rc = UDP_Write(fd, &addr, (char*)pInstr, sizeof(instr_t));
    if(rc <= 0){
      if(debug == 1){
	printf("udp_write failed\n");
      }
      return -1;
    }

    FD_ZERO(&sfd);
    FD_SET(fd, &sfd);
        
    //wait for 5 seconds
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;

    reSend = select(fd + 1, &sfd, NULL, NULL, &timeOut);
    
    if(1==reSend){
      rc = UDP_Read(fd, &addr2, (char*)pRInstr, sizeof(instr_t));
      if(rc <= 0){
	if(debug == 1){
	  printf("udp_write failed\n");
	}
	return -1;
      }
    }
  }
  //strcpy(buffer, pRInstr->buffer);
  memcpy(buffer, pRInstr->buffer,BUFFER_SIZE);
  
  int toBeReturned = pRInstr->rV;
  //  int toBeReturned = atoi(buffer);
  return toBeReturned;
}

int MFS_Creat(int pinum, int type, char *name){
  if(pinum < 0 || name == NULL 
     || (type != MFS_DIRECTORY && type != MFS_REGULAR_FILE)){
    return -1;
  }
  int reSend;
  reSend=0;

  char buffer[sizeof(int)];
  instr_t* pInstr = (instr_t*)malloc(sizeof(instr_t));
  pInstr->type = TYPE_CREAT;
  // TODO: fill in pInstr
  pInstr->inum = pinum;
  pInstr->creatType = type;
  strcpy(pInstr->name, name);
  //memcpy(pInstr->name, name,PATH_MAX);


  while(0==reSend){
    rc = UDP_Write(fd, &addr, (char*)pInstr, sizeof(instr_t));
    if(rc <= 0){
      if(debug == 1){
	printf("udp_write failed\n");
      }
      return -1;
    }

    FD_ZERO(&sfd);
    FD_SET(fd, &sfd);
        
    //wait for 5 seconds
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;

    reSend = select(fd + 1, &sfd, NULL, NULL, &timeOut);

    if(1==reSend){
 
      rc = UDP_Read(fd, &addr2, buffer, sizeof(int));
      if(rc <= 0){
	if(debug == 1){
	  printf("udp_write failed\n");
	}
	return -1;
      }
    }
  }
  int toBeReturned = atoi(buffer);
  //  int toBeReturned = atoi(buffer);
  return toBeReturned;
}

int MFS_Unlink(int pinum, char *name){
  if(pinum < 0 || name == NULL){
    return -1;
  }
  int reSend;
  reSend=0;

  char buffer[sizeof(int)];
  instr_t* pInstr = (instr_t*)malloc(sizeof(instr_t));
  pInstr->type = TYPE_UNLINK;
  // TODO: fill in pInstr
  pInstr->inum = pinum;
  strcpy(pInstr->name, name);
  //memcpy(pInstr->name, name,PATH_MAX);


  while(0==reSend){
    rc = UDP_Write(fd, &addr, (char*)pInstr, sizeof(instr_t));
    if(rc <= 0){
      if(debug == 1){
	printf("udp_write failed\n");
      }
      return -1;
    }


    FD_ZERO(&sfd);
    FD_SET(fd, &sfd);
        
    //wait for 5 seconds
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;

    reSend = select(fd + 1, &sfd, NULL, NULL, &timeOut);

    if(1==reSend){
      rc = UDP_Read(fd, &addr2, buffer, sizeof(int));
      if(rc <= 0){
	if(debug == 1){
	  printf("udp_write failed\n");
	}
	return -1;
      }
    }
  }
  int toBeReturned = atoi(buffer);
  return toBeReturned;
}


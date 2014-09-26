#include "cs537.h"
#include "request.h"
#include <pthread.h>

//TODO: nothing

// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

list_t requestBuffer;
char schdalg[8]; 
int epochSize;

pthread_cond_t      fill= PTHREAD_COND_INITIALIZER;
pthread_cond_t      empty= PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

// CS537: TODO make it parse the new arguments too
void getargs(int* port, int* numThreads, int* numBuffers, char* schdalg, int* epochSize, int argc, char* argv[])
{
  if (argc != 5 && argc != 6) {
    fprintf(stderr, "Usage: %s <port> <threads> <buffers> <schdalg> <N (for SFF-SB only)>\n", argv[0]);
    exit(1);
  }
  if((0 != strcmp(argv[4], "FIFO")) && (0 != strcmp(argv[4], "SFF")) && (0 != strcmp(argv[4], "SFF-BS"))){
    fprintf(stderr, "Usage: %s <port> <threads> <buffers> <schdalg> <N (for SFF-SB only)>\n", argv[0]);
    exit(1);      
  }
  *port = atoi(argv[1]);
  *numThreads = atoi(argv[2]);
  *numBuffers = atoi(argv[3]);
  strcpy(schdalg, argv[4]);
  if(strcmp(argv[4], "SFF-BS") == 0){
    if(argc == 5){
      fprintf(stderr, "Usage: %s <port> <threads> <buffers> <schdalg> <N (for SFF-SB only)>\n", argv[0]);
    } else {
      if(epochSize <=0){
	fprintf(stderr, "Usage: %s <port> <threads> <buffers> <schdalg> <N (for SFF-SB only)>\n", argv[0]);
      }
      *epochSize = atoi(argv[5]);
    }    
  }
  /* check validity of arguments */
  if(port <= 0 || numThreads <= 0 || numBuffers <= 0){
    fprintf(stderr, "Usage: %s <port> <threads> <buffers> <schdalg> <N (for SFF-SB only)>\n", argv[0]);
  }
}

/******************************
 * the working thread function
 ******************************/
void* workThread (void* args)
{
  pThread_t* thisThread = (pThread_t*) args;
  while(1){
    pthread_mutex_lock(&mutex); 
    while (0 == Counter_GetValue(&requestBuffer.volume)) {
      pthread_cond_wait(&fill, &mutex);
    }
    request_t* newRequest;
    newRequest = List_Dequeue(&requestBuffer, schdalg,epochSize);
    // mark dispatch time
    Request_UpdateStat(newRequest, "dispatch");
    pthread_cond_signal(&empty);
    pthread_mutex_unlock(&mutex);
    printf("Handling request: %s\n", newRequest->filename);
    requestHandle(newRequest, thisThread);
    Close(newRequest->connfd);
  }
}

/******************************
 * thread initializing function
 ******************************/
void Thread_Init(pThread_t* thread, int id){
  int err;
  thread = (pThread_t*) malloc(sizeof(pThread_t));
  void* arg = (void*) thread;
  err = pthread_create(&(thread->t_id), NULL, workThread, arg);
  thread->id = id;
  thread->count = 0;
  thread->staticCount = 0;
  thread->dynamicCount = 0;
  if (err != 0){
    printf("can't create thread: %s\n", strerror(err));
  }    
}


/*****************************
 * main method (master thread)
 *****************************/
int main(int argc, char *argv[]){
  int listenfd, connfd, port, clientlen, numThreads, numBuffers;
  
  struct sockaddr_in clientaddr;
  
  getargs(&port, &numThreads, &numBuffers, schdalg, &epochSize, argc, argv);

  List_Init(&requestBuffer);
  // create thread pool
  pThread_t** threadPool = (pThread_t**) malloc(sizeof(pThread_t*) * numThreads);
  int i = 0;
  // initialize threads
  for( ; i<numThreads; i++){
    Thread_Init(threadPool[i], i); // calls pthread_create
  }
  // start listening port
  listenfd = Open_listenfd(port);

  while (1) {
    clientlen = sizeof(clientaddr);  
    connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    // 
    // CS537: In general, don't handle the request in the main thread.
    // TODO In multi-threading mode, the main thread needs to
    // save the relevant info in a buffer and have one of the worker threads
    // do the work.
    //
    if(connfd>0){
      request_t* req = (request_t*) malloc(sizeof(request_t));
      req->connfd = connfd;
      req->age = 0;
      gettimeofday(&req->arrivalTime, &req->tz);
      /* read instructions of request */
      Rio_readinitb(&req->rio, connfd);
      Rio_readlineb(&req->rio, req->buf, MAXLINE);
      /* parse request */
      sscanf(req->buf, "%s %s %s", req->method, req->uri, req->version);
      printf("%s %s %s\n", req->method, req->uri, req->version);
      /* write info into request_t */
      req->is_static = requestParseURI(req->uri, req->filename, req->cgiargs);
      struct stat fileStat;
      stat(req->filename, &fileStat);
      req->fileSize = fileStat.st_size;
      /* save info to request buffer */
      pthread_mutex_lock(&mutex); 
      while (numBuffers == Counter_GetValue(&requestBuffer.volume)){
	pthread_cond_wait(&empty, &mutex);
      }
      List_Enqueue(&requestBuffer, req, schdalg);
      pthread_cond_signal(&fill);
      pthread_mutex_unlock(&mutex);
    }
  }
}


    


 

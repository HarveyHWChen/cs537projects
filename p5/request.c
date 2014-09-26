//
// request.c: Does the bulk of the work for the web server.
// 

#include "cs537.h"
#include "request.h"

void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];
    
    printf("Request ERROR\n");
    
    // Create the body of the error message
    sprintf(body, "<html><title>CS537 Error</title>");
    sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr>CS537 Web Server\r\n", body);
    
    // Write out the header information for this response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
    
    sprintf(buf, "Content-Type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
    
    sprintf(buf, "Content-Length: %d\r\n\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    printf("%s", buf);
    
    // Write out the content
    Rio_writen(fd, body, strlen(body));
    printf("%s", body);
    
}


//
// Reads and discards everything up to an empty text line
//
void requestReadhdrs(rio_t *rp)
{
    char buf[MAXLINE];
    
    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
	Rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int requestParseURI(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;
    
    if (!strstr(uri, "cgi")) {
	// static
	strcpy(cgiargs, "");
	sprintf(filename, ".%s", uri);
	if (uri[strlen(uri)-1] == '/') {
	    strcat(filename, "home.html");
	}
	return 1;
    } else {
	// dynamic
	ptr = index(uri, '?');
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	} else {
	    strcpy(cgiargs, "");
	}
	sprintf(filename, ".%s", uri);
	return 0;
    }
}

//
// Fills in the filetype given the filename
//
void requestGetFiletype(char *filename, char *filetype)
{
    if (strstr(filename, ".html")) 
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif")) 
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg")) 
	strcpy(filetype, "image/jpeg");
    else 
	strcpy(filetype, "test/plain");
}

void requestServeDynamic(request_t* req, pThread_t* thisThread)
{
    char buf[MAXLINE], *emptylist[] = {NULL};
    thisThread->count++;
    thisThread->dynamicCount++;        
    // The server does only a little bit of the header.  
    // The CGI script has to finish writing out the header.
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%s Server: Tiny Web Server\r\n", buf);
    
    /* CS537: Your statistics go here -- fill in the 0's with something useful! */
    sprintf(buf, "%s Stat-req-arrival: %f\r\n", buf, (double)(req->arrivalTime.tv_sec + req->arrivalTime.tv_usec/1000.0) * 1000); 
    double dispatch = req->dispatchTime.tv_sec - req->arrivalTime.tv_sec + ((req->dispatchTime.tv_usec - req->arrivalTime.tv_usec)/1000.0);
    sprintf(buf, "%s Stat-req-dispatch: %f\r\n", buf, dispatch);
    sprintf(buf, "%s Stat-thread-id: %d\r\n", buf, thisThread->id);
    sprintf(buf, "%s Stat-thread-count: %d\r\n", buf, thisThread->count);
    sprintf(buf, "%s Stat-thread-static: %d\r\n", buf, thisThread->staticCount);
    sprintf(buf, "%s Stat-thread-dynamic: %d\r\n", buf, thisThread->dynamicCount);
    
    
    Rio_writen(req->connfd, buf, strlen(buf));
    
    if (Fork() == 0) {
	/* Child process */
	Setenv("QUERY_STRING", req->cgiargs, 1);
	/* When the CGI process writes to stdout, it will instead go to the socket */
	Dup2(req->connfd, STDOUT_FILENO);
	Execve(req->filename, emptylist, environ);
    }
    Wait(NULL);
}


void requestServeStatic(request_t* req, pThread_t* thisThread)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
    char tmp = 0;
    int i;
    requestGetFiletype(req->filename, filetype);
    
    srcfd = Open(req->filename, O_RDONLY, 0);
    
    // Rather than call read() to read the file into memory, 
    // which would require that we allocate a buffer, we memory-map the file
    srcp = Mmap(0, req->fileSize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    struct timeval read1;
    struct timeval read2;
    gettimeofday(&read1, &req->tz);
    // The following code is only needed to help you time the "read" given 
    // that the file is memory-mapped.  
    // This code ensures that the memory-mapped file is brought into memory 
    // from disk.
    
    // When you time this, you will see that the first time a client 
    //requests a file, the read is much slower than subsequent requests.
    for (i = 0; i < req->fileSize; i++) {
	tmp += *(srcp + i);
    }
    gettimeofday(&read2, &req->tz);
    Request_UpdateStat(req, "complete");
    thisThread->count++;
    thisThread->staticCount++;            
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%s Server: CS537 Web Server\r\n", buf);
    
    // CS537: Your statistics go here -- fill in the 0's with something useful!
    sprintf(buf, "%s Stat-req-arrival: %f\r\n", buf, (double)(req->arrivalTime.tv_sec + req->arrivalTime.tv_usec/1000.0) * 1000);
    double dispatch = req->dispatchTime.tv_sec - req->arrivalTime.tv_sec + ((req->dispatchTime.tv_usec - req->arrivalTime.tv_usec)/1000.0);
    sprintf(buf, "%s Stat-req-dispatch: %f\r\n", buf, dispatch);
    double read = read2.tv_sec - read1.tv_sec + ((read2.tv_usec - read1.tv_usec)/1000.0);
    sprintf(buf, "%s Stat-req-read: %f\r\n", buf, read);
    double complete = req->completeTime.tv_sec - req->arrivalTime.tv_sec + ((req->completeTime.tv_usec - req->arrivalTime.tv_usec)/1000.0);
    sprintf(buf, "%s Stat-req-complete: %f\r\n", buf, complete);
    sprintf(buf, "%s Stat-req-age: %d\r\n", buf, req->age);
    sprintf(buf, "%s Stat-thread-id: %d\r\n", buf, thisThread->id);
    sprintf(buf, "%s Stat-thread-count: %d\r\n", buf, thisThread->count);
    sprintf(buf, "%s Stat-thread-static: %d\r\n", buf, thisThread->staticCount);
    sprintf(buf, "%s Stat-thread-dynamic: %d\r\n", buf, thisThread->dynamicCount);
    
    sprintf(buf, "%s Content-Length: %d\r\n", buf, req->fileSize);
    sprintf(buf, "%s Content-Type: %s\r\n\r\n", buf, filetype);
    
    Rio_writen(req->connfd, buf, strlen(buf));
    
    //  Writes out to the client socket the memory-mapped file 
    Rio_writen(req->connfd, srcp, req->fileSize);
    Munmap(srcp, req->fileSize);

}

// handle a request
// TODO: modify this as request_t input
void requestHandle(request_t* req, pThread_t* thisThread)
{
  /* master thread has already read info from connection port */
  /* and saved those info as well */
  struct stat sbuf;

  if (strcasecmp(req->method, "GET")) {
    requestError(req->connfd, req->method, "501", "Not Implemented", 
		 "CS537 Server does not implement this method");
    return;
  }
  requestReadhdrs(&req->rio);
  
  if (stat(req->filename, &sbuf) < 0) {
    requestError(req->connfd, req->filename, "404", "Not found", "CS537 Server could not find this file");
    return;
  }
  
  if (req->is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      requestError(req->connfd, req->filename, "403", "Forbidden", "CS537 Server could not read this file");
      return;
    }
    requestServeStatic(req, thisThread);
  } else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      requestError(req->connfd, req->filename, "403", "Forbidden", "CS537 Server could not run this CGI program");
      return;
    }
    printf("-------------------------->\n");
    requestServeDynamic(req, thisThread);
  }
}



#ifndef __REQUEST_H__

void requestHandle(request_t *req, pThread_t* thisThread);
int requestParseURI(char *uri, char *filename, char *cgiargs) ;
#endif

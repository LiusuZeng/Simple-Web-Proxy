/*
 * proxy.c - Web proxy for COMPSCI 512
 *
 */

#include <stdio.h>
#include "csapp.h"
#include <pthread.h>

#define   FILTER_FILE   "proxy.filter"
#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE	"proxy.debug"


/*============================================================
 * function declarations
 *============================================================*/

int  find_target_address(char * uri,
			 char * target_address,
			 char * path,
			 int  * port);


void  format_log_entry(char * logstring,
		       int sock,
		       char * uri,
		       int size);
		       
void *forwarder(void* args);
void *webTalk(void* args);
void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort);
void ignore();

int debug;
int proxyPort;
int debugfd;
int logfd;
pthread_mutex_t mutex;

/* main function for the proxy program */

int main(int argc, char *argv[])
{
  int count = 0;
  int listenfd, connfd, clientlen, optval, serverPort, i;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *haddrp;
  sigset_t sig_pipe; 
  pthread_t tid;
  int *args;
  
  if (argc < 2) {
    printf("Usage: %s port [debug] [webServerPort]\n", argv[0]);
    exit(1);
  }
  if(argc == 4)
    serverPort = atoi(argv[3]);
  else
    serverPort = 80;
  
  Signal(SIGPIPE, ignore);
  
  if(sigemptyset(&sig_pipe) || sigaddset(&sig_pipe, SIGPIPE))
    unix_error("creating sig_pipe set failed");
  if(sigprocmask(SIG_BLOCK, &sig_pipe, NULL) == -1)
    unix_error("sigprocmask failed");
  
  proxyPort = atoi(argv[1]);

  if(argc > 2)
    debug = atoi(argv[2]);
  else
    debug = 0;


  /* start listening on proxy port */

  listenfd = Open_listenfd(proxyPort);

  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  if(debug) debugfd = Open(DEBUG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);

  logfd = Open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);    


  /* if writing to log files, force each thread to grab a lock before writing
     to the files */
  
  pthread_mutex_init(&mutex, NULL);
  
  while(1) {

    clientlen = sizeof(clientaddr);

    /* accept a new connection from a client here */

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
    /* you have to write the code to process this new client request */
    /* create a new thread (or two) to process the new connection */
    
    // [ZLS] Construct input param for the new thread
    args = (int*)Malloc(sizeof(int)*2);
    *args = connfd;
    *(args+1) = serverPort;
    Pthread_create(&tid, NULL, (void*)(webTalk), (void*)(args));
    Pthread_detach(tid); // [ZLS] make it unrealted to the main listening thread
  }
  
  if(debug) Close(debugfd);
  Close(logfd);
  pthread_mutex_destroy(&mutex);
  
  return 0;
}

/* a possibly handy function that we provide, fully written */

void parseAddress(char* url, char* host, char** file, int* serverPort)
{
	char *point1;
        char *point2;
        char *saveptr;

	if(strstr(url, "http://"))
		url = &(url[7]);
	*file = strchr(url, '/');
	
	strcpy(host, url);

	/* first time strtok_r is called, returns pointer to host */
	/* strtok_r (and strtok) destroy the string that is tokenized */

	/* get rid of everything after the first / */

	strtok_r(host, "/", &saveptr);

	/* now look to see if we have a colon */

	point1 = strchr(host, ':');
	if(!point1) {
		*serverPort = 80;
		return;
	}
	
	/* we do have a colon, so get the host part out */
	strtok_r(host, ":", &saveptr);

	/* now get the part after the : */
	*serverPort = atoi(strtok_r(NULL, "/",&saveptr));
}



/* this is the function that I spawn as a thread when a new
   connection is accepted */

/* you have to write a lot of it */

void *webTalk(void* args)
{
  int numBytes, lineNum, serverfd, clientfd, serverPort;
  int tries = 5;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  char host[MAXLINE];
  char* file;
  char* url;
  char logString[MAXLINE];
  char *token, *cmd, *version, *saveptr;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");
  
  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  free(args);
  
  Rio_readinitb(&client, clientfd);
  
  // Determine protocol (CONNECT or GET)
  // [ZLS] read 1st line
  numBytes = Rio_readlineb(&client, buf1, MAXLINE);
  if(strcmp(buf1, "\0") == 0) {
    return NULL;
  }
  //printf("What's in buf1: %s\n", buf1);
  // [ZLS] parse to get the protocol: GET or CONNECT
  cmd = strtok_r(buf1, " \r\n", &saveptr);
  // [ZLS] parse to get request_url
  url = strtok_r(NULL, " \r\n", &saveptr);
  // [ZLS] parse to get version
  version = strtok_r(NULL, " \r\n", &saveptr);
  // [ZLS] parse url to get hostname, filepath, portnumber
  parseAddress(url, host, &file, &serverPort);
  if(file == NULL) file = slash;
  if(cmd == NULL) printf("Error in cmd!\n");
  //
  //printf("Req Type is: %s\n", cmd);
  //printf("File part: %s\n", file);
  //
  if(strncmp(cmd, "POST", 4) == 0) {
    serverfd = -1;
    while(tries--) {
      serverfd = Open_clientfd(host, serverPort);
      if(serverfd == -1) break;
    }
    //
    if(serverfd <= 0) return NULL;
    //
    Rio_readinitb(&server, serverfd);
    //
    sprintf(buf3, "%s %s %s\r\n", cmd, file, version);
    printf("%s\n", buf3); // disp POST details
    Rio_writen(serverfd, buf3, strlen(buf3));
    // POST: Transfer remainder of the request
    while((numBytes = Rio_readlineb(&client, buf2, MAXLINE)) > 0) {
      //
      printf("[%s --> headers:] %s", cmd, buf2);
      //
      if(strncmp(buf2, "Proxy-Connection: keep-alive\r\n", strlen("Proxy-Connection: keep-alive\r\n")) != 0) Rio_writen(serverfd, buf2, numBytes);
      else Rio_writen(serverfd, "Proxy-Connection: close\r\n", strlen("Proxy-Connection: close\r\n"));
      //
      if(strcmp(buf2, "\r\n") == 0) break;
    }
    // Send part of the message body that is already in the internal buffer
    memcpy(buf2, client.rio_bufptr, client.rio_cnt);
    //
    printf("<Message in buffer:> %s\n", buf2);
    //
    Rio_writen(serverfd, buf2, client.rio_cnt);
    // Send the rest of the message body
    while((numBytes = Rio_readp(clientfd, buf2, MAXLINE)) > 0) {
      Rio_writen(serverfd, buf2, numBytes);
      //
      printf("<Message in the rest:> %s\n", buf2);
      //
    }
    //
    printf("Finish Sending Message body!!!\n");
    //
    // Now receive response
    while((numBytes = Rio_readp(serverfd, buf3, MAXLINE)) > 0) {
      Rio_writen(clientfd, buf3, numBytes);
      //
      printf("[%s --> Response:] %s\n\n", buf3);
      //
      byteCount += numBytes;
    }
    //
    printf("End of POST!!!\n");
    //
    Close(clientfd);
    Close(serverfd);
  }
  //
  else if(strncmp(cmd, "GET", 3) == 0) {
    // GET: open connection to webserver (try several times, if necessary)
    serverfd = -1;
    while(tries--) {
      serverfd = Open_clientfd(host, serverPort);
      if(serverfd != -1) break; 
    }
    // [ZLS] Check if a connection the remote server has been created
    if(serverfd <= 0) {
      //dns_error("Cannot establish socket to the remote server!");
      return NULL;
    }
    //
    Rio_readinitb(&server, serverfd);
    //
    /* GET: Transfer first header to webserver */
    sprintf(buf3, "%s %s %s\r\n", cmd, file, version);
    printf("%s\n", buf3); // disp GET details
    Rio_writen(serverfd, buf3, strlen(buf3));
    // GET: Transfer remainder of the request
    while((numBytes = Rio_readlineb(&client, buf2, MAXLINE)) > 0) {
      if(strncmp(buf2, "Connection: keep-alive\r\n", strlen("Connection: keep-alive\r\n")) != 0) Rio_writen(serverfd, buf2, numBytes);
      else Rio_writen(serverfd, "Connection: close\r\n", strlen("Connection: close\r\n")); // Manually disable the keep-alive connections
      //
      //printf("Remained headers: Num from readlineb: %d, Num from strlen: %d.\n", numBytes, (int)strlen(buf2));
      //
      if(strcmp(buf2, "\r\n") == 0) {
	//printf("End of sending get request\n");
	break;
      }
    }

    //
    // GET: now receive the response
    // [ZLS] This is where it gets slooooow....
    while((numBytes = Rio_readp(serverfd, buf3, MAXLINE)) > 0) {
      Rio_writen(clientfd, buf3, numBytes);
      //
      //printf("Recv Response: Num from readp: %d, Num from strlen: %d.\n", numBytes, (int)strlen(buf3));
      //
      byteCount += numBytes;
      //
    }
    // [ZLS] End of GET, put it in log file
    format_log_entry(logString, serverfd, url, byteCount);
    pthread_mutex_lock(&mutex);
    Write(logfd, logString, strlen(logString));
    pthread_mutex_unlock(&mutex);
    // [ZLS] Close sockets
    // Retrieve fd
    Close(clientfd);
    Close(serverfd);
    //shutdown(clientfd, 1);
    //shutdown(serverfd, 1);
    //
    //printf("End of GET! Bytes: %d.\n\n\n", byteCount);
  }
  // CONNECT: call a different function, securetalk, for HTTPS
  else if(strncmp(cmd, "CONNECT", 7) == 0) {
    secureTalk(clientfd, client, host, version, serverPort);
  }
  else {
    printf("%s: Request type not supported!\n", cmd);
  }
  //
  Pthread_exit(NULL);
}


/* this function handles the two-way encrypted data transferred in
   an HTTPS connection */

void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort)
{
  int serverfd, numBytes1, numBytes2;
  int tries;
  int byteCount = 0;
  rio_t server;
  char buf1[MAXLINE], buf2[MAXLINE];
  pthread_t tid;
  int *args = (int*)Malloc(sizeof(int)*2);
  
  if (serverPort == proxyPort)
    serverPort = 443;
  
  /* Open connecton to webserver */
  /* clientfd is browser */
  /* serverfd is server */
  
  serverfd = Open_clientfd(inHost, serverPort);
  //printf("After open socket: host: %s, port: %d, serverID: %d\n", inHost, serverPort, serverfd);
  if(serverfd < 0) return;
  /* let the client know we've connected to the server */
  else {
    sprintf(buf1, "%s 200 Connection Established\r\n\r\n", version);
    //printf("buf1: %s\n", buf1);
    Rio_writen(clientfd, buf1, strlen(buf1));
    /* spawn a thread to pass bytes from origin server through to client */
    *args = clientfd;
    *(args+1) = serverfd;
    Pthread_create(&tid, NULL, (void*)(forwarder), (void*)(args));
    /* now pass bytes from client to server */
    while((numBytes1 = Rio_readp(clientfd, buf2, MAXLINE)) > 0) {
        Rio_writen(serverfd, buf2, numBytes1);
	//
	printf("C2S: Num from readp: %d.\n", numBytes1);
	//
	byteCount += numBytes1;
    }
    //
    Pthread_join(tid, NULL);
    // retrieve fd or not???
    Close(clientfd);
    Close(serverfd);
    //shutdown(clientfd, 1);
    //shutdown(serverfd, 1);
    //
    //printf("End of CONNECT! Clent2ServerBytes: %d\n\n\n", byteCount);
    //
    return;
  }
}

/* this function is for passing bytes from origin server to client */

void *forwarder(void* args)
{
  int numBytes, lineNum, serverfd, clientfd;
  int byteCount = 0;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0];
  serverfd = ((int*)args)[1];
  Free(args);

  while((numBytes = Rio_readp(serverfd, buf1, MAXLINE)) > 0) {
    /* serverfd is for talking to the web server */
    /* clientfd is for talking to the browser */
    Rio_writen(clientfd, buf1, numBytes);
    //
    printf("S2C: Num from readp: %d.\n", numBytes);
    //
    byteCount += numBytes;
  }
  //
  //printf("End of CONNECT! Server2ClientBytes: %d\n\n\n", byteCount);
  //
  Pthread_exit(NULL);
}


void ignore()
{
	;
}


/*============================================================
 * url parser:
 *    find_target_address()
 *        Given a url, copy the target web server address to
 *        target_address and the following path to path.
 *        target_address and path have to be allocated before they 
 *        are passed in and should be long enough (use MAXLINE to be 
 *        safe)
 *
 *        Return the port number. 0 is returned if there is
 *        any error in parsing the url.
 *
 *============================================================*/

/*find_target_address - find the host name from the uri */
int  find_target_address(char * uri, char * target_address, char * path,
                         int  * port)

{
  //  printf("uri: %s\n",uri);
    if (strncasecmp(uri, "http://", 7) == 0) {
	char * hostbegin, * hostend, *pathbegin;
	int    len;
       
	/* find the target address */
	hostbegin = uri+7;
	hostend = strpbrk(hostbegin, " :/\r\n");
	if (hostend == NULL){
	  hostend = hostbegin + strlen(hostbegin);
	}
	
	len = hostend - hostbegin;

	strncpy(target_address, hostbegin, len);
	target_address[len] = '\0';

	/* find the port number */
	if (*hostend == ':')   *port = atoi(hostend+1);

	/* find the path */

	pathbegin = strchr(hostbegin, '/');

	if (pathbegin == NULL) {
	  path[0] = '\0';
	  
	}
	else {
	  pathbegin++;	
	  strcpy(path, pathbegin);
	}
	return 0;
    }
    target_address[0] = '\0';
    return -1;
}



/*============================================================
 * log utility
 *    format_log_entry
 *       Copy the formatted log entry to logstring
 *============================================================*/

void format_log_entry(char * logstring, int sock, char * uri, int size)
{
    time_t  now;
    char    buffer[MAXLINE];
    struct  sockaddr_in addr;
    unsigned  long  host;
    unsigned  char a, b, c, d;
    int    len = sizeof(addr);

    now = time(NULL);
    strftime(buffer, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (getpeername(sock, (struct sockaddr *) & addr, &len)) {
      /* something went wrong writing log entry */
      printf("getpeername failed\n");
      return;
    }

    host = ntohl(addr.sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", buffer, a,b,c,d, uri, size);
}

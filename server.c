#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "support.h"

/*
 * help() - Print a help message
 */
const int PUT = 1;
const int GET = 2;

struct request{
  int type;
  char* name;
  int size_bytes;
};

struct fileBuffer{
    char * name;
    int size;
    int eviction_score;
    char* contents;
};

struct fileBuffer ** cache;
int maxCacheSize;
int cacheSize;

void help(char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("Initiate a network file server\n");
    printf("  -l    number of entries in cache\n");
    printf("  -p    port on which to listen for connections\n");
}

/*
 * die() - print an error and exit the program
 */
void die(const char *msg1, char *msg2) {
    fprintf(stderr, "%s, %s\n", msg1, msg2);
    exit(0);
}

/*
 * open_server_socket() - Open a listening socket and return its file
 *                        descriptor, or terminate the program
 */
int open_server_socket(int port) {
    int                listenfd;    /* the server's listening file descriptor */
    struct sockaddr_in addrs;       /* describes which clients we'll accept */
    int                optval = 1;  /* for configuring the socket */

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("Error creating socket: ", strerror(errno));

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        die("Error configuring socket: ", strerror(errno));

    /* Listenfd will be an endpoint for all requests to the port from any IP
       address */
    bzero((char *) &addrs, sizeof(addrs));
    addrs.sin_family = AF_INET;
    addrs.sin_addr.s_addr = htonl(INADDR_ANY);
    addrs.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr *)&addrs, sizeof(addrs)) < 0)
        die("Error in bind(): ", strerror(errno));

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, 1024) < 0)  // backlog of 1024
        die("Error in listen(): ", strerror(errno));

    return listenfd;
}

/*
 * handle_requests() - given a listening file descriptor, continually wait
 *                     for a request to come in, and when it arrives, pass it
 *                     to service_function.  Note that this is not a
 *                     multi-threaded server.
 */
void handle_requests(int listenfd, void (*service_function)(int, int), int param) {
    while (1) {
        /* block until we get a connection */
        struct sockaddr_in clientaddr;
        int clientlen = sizeof(clientaddr);
        int connfd;
        if ((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0)
            die("Error in accept(): ", strerror(errno));

        /* print some info about the connection */
        struct hostent *hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                           sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hp == NULL) {
            fprintf(stderr, "DNS error in gethostbyaddr() %d\n", h_errno);
            exit(0);
        }
        char *haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to %s (%s)\n", hp->h_name, haddrp);

        /* serve requests */
        service_function(connfd, param);

        /* clean up, await new connection */
        if (close(connfd) < 0)
            die("Error in close(): ", strerror(errno));
    }
}

//finds and frees the oldest cached file
struct fileBuffer * removeOldest(){
    struct fileBuffer * oldest = NULL;
    int high = 0;
    int i = 0;
    while(i < cacheSize){
        if(cache[i] == NULL){
            return cache[i];
        }else{
            if(cache[i]->eviction_score > high){
                high = cache[i]->eviction_score;
                oldest = cache[i];
            }
        }

        i++;
    }
    if(oldest != NULL){
        free(oldest);
    }
    //returns the free pointer to be used for a more recent file
    return oldest;
}

struct fileBuffer * getFileBuffer(char * filename){ //returns null if nothing is found
    int i;
    //finds file with the same name
    for(i = 0; i < cacheSize; i++){
        if(cache[i] != NULL && !strcmp(cache[i]->name, filename)){
            return cache[i];
        }
    }
    return NULL;
}

void updateEvictionScores(struct fileBuffer * inUse){

    int i = 0;
    //increments all the eviction scores
    while(i < cacheSize && cache[i] != NULL){
        cache[i]->eviction_score += 1;
        i++;
    }
    //sets the currently used cached file to zero
    inUse->eviction_score = 0;
}

void lruCacheSetup(int size){
    //initializes the cache max size and current size
    cache = malloc(sizeof(struct fileBuffer) * size);
    maxCacheSize = size;
    cacheSize = 0;
}



void addFileBuffer(struct fileBuffer * myFile){
    //point to a cached file with the same name or NULL if it doesnt exist
    struct fileBuffer * oldVersion = getFileBuffer(myFile->name);

    if(oldVersion != NULL){             //replaces a file with the same name in the cache
        oldVersion = myFile;
    }else if(cacheSize < maxCacheSize){ //adds a new file to the cache
        cache[cacheSize] = myFile;
        cacheSize++;
    }else{                              //replaces the oldest file in the cache
        struct fileBuffer * replace  = removeOldest();
        replace = myFile;
    }
    updateEvictionScores(myFile);
}

int popType(char* cmd, struct request *myRequest){
  printf("\ncommand is %s\n", cmd);  
  if(!strcmp(cmd, "PUT\n")){
        myRequest->type = PUT;
        return 1;
    }else if(!strcmp(cmd, "GET\n")){
        myRequest->type = GET;
        return 1;
    }
    return 0;
}

int popName(char* name, struct request *myRequest){
  int flag = 0;
  int len = strlen(name);
  if(name[len - 1] == '\n'){
      name[len - 1] = '\0';
      flag = 1;
  }
  if(!strlen(name)){ //name is empty
        return 0;
    }
    if(myRequest->type == GET){
        if(fopen(name, "r") == NULL){
            return 0;
        }
    }
    myRequest->name = strdup(name);
    if(flag){
        name[len - 1] = '\n';
    }
    return 1;
}

int popSize(char *sizeStr, struct request *myRequest){
    int sizeInt;
     if(!sscanf(sizeStr, "%d", &sizeInt)){
         return 0;
     }else if(sizeInt < 1){
         return 0;
     }
     myRequest->size_bytes = sizeInt;
     return 1;
}

void response(int connfd, int type, int OK, struct fileBuffer *fb){
  //For GET type = 1
  //For PUT type = 2
  //If OK = 1 then the GET/PUT was successful, OK = 0 if not
  //For a GET request the fb will hold all the data that needs to be 
  //    transfered to the client

  //if is PUT
  ssize_t nsofar;
  ssize_t nremain;
  if(type == 2){
    if((nsofar = write(connfd, "OK\n", 3)) <= 0){
      if (errno != EINTR){
	die("Write error: ", strerror(errno));
      }
      nsofar = 0;
    }
    return;
  }
  
  //if GET
  // append into one string
  int size = 3;
  if(type = 1){
    size += (strlen(fb->name) + 1 + strlen(fb->size) + 1 strlen(fb->contents));
    char sendArray[size];
    
    
  }
}

  nremain = 3;
  bufp = buf;
  while (nremain > 0) {
    /* write some data; swallow EINTRs */
    if ((nsofar = write(connfd, bufp, nremain)) <= 0) {
      if (errno != EINTR)
	die("Write error: ", strerror(errno));
      nsofar = 0;
    }
    nremain -= nsofar;
    bufp += nsofar;
  }
}

/*
 * file_server() - Read a request from a socket, satisfy the request, and
 *                 then close the connection.
 */
void file_server(int connfd, int lru_size) {
    /* TODO: set up a few static variables here to manage the LRU cache of
       files */
       lruCacheSetup(lru_size);
    /* TODO: replace following sample code with code that satisfies the
       requirements of the assignment */

    /* sample code: continually read lines from the client, and send them
       back to the client immediately */
  int requestCount = 0;
  int check;
  struct request *myRequest = malloc(sizeof(struct request));
  char * currentFileContents;
  while (1) {
        const int MAXLINE = 8192;
        char      buf[MAXLINE];   /* a place to store text from the client */
        bzero(buf, MAXLINE);

        /* read from socket, recognizing that we may get short counts */
        char *bufp = buf;              /* current pointer into buffer */
        ssize_t nremain = MAXLINE;     /* max characters we can still read */
        size_t nsofar;             
	printf("Waiting for next line on %d\n", connfd);
        while (1) {
            /* read some data; swallow EINTRs */
            if ((nsofar = read(connfd, bufp, nremain)) < 0) {
                if (errno != EINTR)
                    die("read error: ", strerror(errno));
		printf("recieved and EINTR\n");
		continue;
            }
	    printf("intial buffer: %s\n", bufp);
            /* end service to this client on EOF */
            if (nsofar == 0) {
                fprintf(stderr, "received EOF\n");
                return;
            }
            /* update pointer for next bit of reading */
            bufp += nsofar;
            nremain -= nsofar;
            if (*(bufp-1) == '\n') {
	      break;
	    }
	   
	    printf("got to end of while loop\n");
	    
	}
	
	printf("contents of bufp: %s\ncontents of buf: %s", bufp, buf);
	//continue;
    if(requestCount == 3){
        printf("contents of the file: %s\n", bufp);
	//it is a PUT request so write the contents from bufp to the filename specifies in myRequest
	
	//Nate this all you!
	//write to the currentFileContents
	//USE MY SKILZZZ I LEARNED IN THE MOUNTAINNSSSSS

	FILE *putFile;
	putFile = fopen(myRequest->name, "w");
	strcpy(currentFileContents, buf);
	if(putFile != NULL){
	  int writeErr = fputs(currentFileContents, putFile);
	  fclose(putFile);
	  //creating a fileBuffer
	  struct fileBuffer *putBuff = malloc(sizeof(struct fileBuffer));
	  putBuff->name = myRequest->name;
	  putBuff->size = myRequest->size_bytes;
	  putBuff->contents = currentFileContents;
	  addFileBuffer(putBuff);
	  //now server needs to send to the client OK\n
	}
	else{
	  printf("could not open the file");
	  //server needs to send to the client an error message
	}
	//*bufp = 0;
	*(bufp - 1) = '\n';
    }else if(requestCount == 2){
      if(myRequest->type == PUT){
	check = popSize((bufp - nsofar), myRequest);
	if(check){
	  requestCount++;
	  *bufp = 0;
	  printf("size is %d\n", myRequest->size_bytes);
	  currentFileContents = malloc(myRequest->size_bytes);
	}
	else{
	  *bufp = 0;
	  continue;
	}
      }
      else{
	//it is a get request

    //call getFileBuffer if it gives you a real fileBuffer just use that
    //if it is null read in a file as normal and write it to a fileBuffer and the client and pass it to addFileBuffer

	FILE *getFile;
	char ch = 'a';
	int index = 0;
	int size = 0;
        getFile = fopen(myRequest->name, "r");
        if(getFile != NULL){
	  //first get file size
	  while((ch = fgetc(getFile)) != EOF){
	    size++;
	  }
	  fclose(getFile);
	  currentFileContents = malloc(sizeof(char) * size);
	  getFile = fopen(myRequest->name, "r");
	  //Then write to out malloced currentFileContents array
          while((ch = fgetc(getFile)) != EOF){
	    currentFileContents[index] = ch;
	    index++;
	  }
	  index = 0;
          fclose(getFile);
	  //check if the file is already in LRC
	  struct fileBuffer *getBuff = getFileBuffer(myRequest->name);
	  if(!getBuff){
	    //does not exist so need to cread a fileBuffer
	    getBuff = malloc(sizeof(struct fileBuffer));
	    getBuff->name = myRequest->name;
	    getBuff->size = myRequest->size_bytes;
	    getBuff->contents = currentFileContents;
	    addFileBuffer(getBuff);
	    //return the OK to the client w/ the getBuff info
	  }
	  else{
	    //return the OK to the client w/ the getBuff info returned from cache
	  }
        }
        else{
          printf("could not open the file");
          //server needs to send to the client an error message         
        }
	requestCount = 0;
	break;
      }
    }else if(requestCount == 1){
      printf("Filename: %s\n", bufp - nsofar);
      check = popName((bufp - nsofar), myRequest);
      if(check){
	requestCount++;
	*bufp = 0;
	printf("name is %s\n", myRequest->name);
      }
    }else if(requestCount == 0){
      printf("print left nut\nbufp is: %s\n", (bufp - nsofar));
      check = popType((bufp - nsofar), myRequest);
      if(check){ 
	requestCount++;
	*bufp = 0;
	printf("requestType is %d\n", myRequest->type);
      }     
    }
    
    *bufp = 0; //not sure what this does
    
    //This is the code that echos the client input back to it

    /* dump content back to client (again, must handle short counts) */
    /* printf("server received %d bytes\n", MAXLINE-nremain);
    nremain = bufp - buf;
    bufp = buf;
    while (nremain > 0) {
      /* write some data; swallow EINTRs */
    /* if ((nsofar = write(connfd, bufp, nremain)) <= 0) {
	if (errno != EINTR)
	  die("Write error: ", strerror(errno));
	nsofar = 0;
      }
      nremain -= nsofar;
      bufp += nsofar;
    }*/
    
    
  }
}

/*
 * main() - parse command line, create a socket, handle requests
 */
int main(int argc, char **argv) {
    /* for getopt */
    long opt;
    /* NB: the "part 3" behavior only should happen when lru_size > 0 */
    int  lru_size = 10;
    int  port     = 9000;

    check_team(argv[0]);

    /* parse the command-line options.  They are 'p' for port number,  */
    /* and 'l' for lru cache size.  'h' is also supported. */
    while ((opt = getopt(argc, argv, "hl:p:")) != -1) {
        switch(opt) {
          case 'h': help(argv[0]); break;
          case 'l': lru_size = atoi(argv[0]); break;
          case 'p': port = atoi(optarg); break;
        }
    }

    /* open a socket, and start handling requests */
    int fd = open_server_socket(port);
    handle_requests(fd, file_server, lru_size);

    exit(0);
}

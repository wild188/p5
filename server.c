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
char *EOT = "BEAT LAFAYETTE!";

const int PUT = 1;
const int GET = 2;

struct request{
  int type;
  int checkSum;
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
int removeOldest(){
    struct fileBuffer * oldest = NULL;
    int high = 0;
    int i = 0;
    int oldIndex = 0;
    while(i < cacheSize){
        if(cache[i] == NULL){
            return i;
        }else{
            if(cache[i]->eviction_score > high){
                high = cache[i]->eviction_score;
                oldest = cache[i];
                oldIndex = i;
            }
        }

        i++;
    }
    if(oldest != NULL){
        free(oldest);
    }
    
    //returns the free pointer to be used for a more recent file
    return oldIndex;
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
      //free(oldVersion);
      //oldVersion = malloc(sizeof(struct fileBuffer *));
        oldVersion = myFile;
	printf("replaced old version of file in cache\n");
    }else if(cacheSize < maxCacheSize){ //adds a new file to the cache
        cache[cacheSize] = myFile;
        cacheSize++;
	printf("Added file to cache\n");
    }else{                              //replaces the oldest file in the cache
        int oldestI = removeOldest();
	    cache[oldestI] = malloc(sizeof(struct fileBuffer));
	    cache[oldestI] = myFile;
	    printf("Replaced oldest file in position %i\n", oldestI);
    }
    updateEvictionScores(myFile);
}

int popType(char* cmd, struct request *myRequest){
  //printf("\ncommand is %s\n", cmd);  
  if(!strcmp(cmd, "PUT")){
        myRequest->type = PUT;
        myRequest->checkSum = 0;
        return 1;
    }else if(!strcmp(cmd, "GET")){
        myRequest->type = GET;
        myRequest->checkSum = 0;
        return 1;
    }else if(!strcmp(cmd, "PUTC")){
        myRequest->type = PUT;
        myRequest->checkSum = 1;
        return 1;
    }else if(!strcmp(cmd, "GETC")){
        myRequest->type = GET;
        myRequest->checkSum = 1;
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

int checkSum(char *cs, struct request *myRequest, char *contents){
  //Nate use your mountain skills
  unsigned char hash[256];
  bzero(hash, 256);
  char temp[32];
  printf("Contents as hex:\n%x", contents);
  MD5(contents, myRequest->size_bytes + 1, hash);
  sprintf(temp, "%x", hash);
  printf("The contents of contents is:\n%s\nServer hash:\n %s\nclient hash:\n %s\n", contents, temp, cs);
  if(!strcmp(temp, cs)){
    printf("the MD5 hash is: %s\n and matched", hash);
    return 1;
  }
  printf("The hash is %s but it didn't match\n", hash);
  return 0;
}

void response(int connfd, char * output){
  //For GET type = 1
  //For PUT type = 2
  //If OK = 1 then the GET/PUT was successful, OK = 0 if not
  //For a GET request the fb will hold all the data that needs to be 
  //    transfered to the client
  char buf[8192];
  char * bufp = buf;
  bzero(buf, 8192);
  strcat(buf, output);
  strcat(buf, EOT);
  int nremain = strlen(buf);
  int nsofar = 0;
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

int makefile(struct request* myRequest, char * contents, int readSoFar, int connfd){
  int expected = myRequest->size_bytes;
  if(readSoFar < expected && 0){
    //printf("Reading %i more of %i already have:\n%s\n", expected - readSoFar, expected, contents);
    // sleep(2);
    char * contentsp = contents + readSoFar;
    int readCur= 0;
    //printf("Waiting for next line on %d\n", connfd);              
    while (1) {
      /* read some data; swallow EINTRs */
      if ((readCur = read(connfd, contentsp, (expected - readSoFar)) < 0)) {
	if (errno != EINTR)
	  die("read error: ", strerror(errno));
	printf("recieved and EINTR\n");
	continue;
      }
      /* end service to this client on EOF */
      if (readCur == 0) {
	fprintf(stderr, "received no contents\n");
	//return;
      }
	
	readSoFar += readCur;
	/* update pointer for next bit of reading */
      contentsp += readSoFar;
      if (expected <= readSoFar) {
	break;
      }

    }

  }

  printf("contents: %s\n", contents); 
  FILE *putFile;
  putFile = fopen(myRequest->name, "w");
	if(putFile != NULL){
	  //printf("the contents are", contents);
	  int writeErr = fputs(contents, putFile);
	  fclose(putFile);
	  //creating a fileBuffer
	  struct fileBuffer *putBuff = malloc(sizeof(struct fileBuffer));
	  putBuff->name = strdup(myRequest->name);
	  putBuff->size = myRequest->size_bytes;
	  putBuff->contents = strdup(contents);
	  addFileBuffer(putBuff);
	  //now server needs to send to the client OK\n
	  return 1;
	}
	else{
	  printf("could not open the file");
	  //server needs to send to the client an error message
	}
	return 1;
}

/*
 * file_server() - Read a request from a socket, satisfy the request, and
 *                 then close the connection.
 */
void file_server(int connfd, int lru_size) {
    /* TODO: set up a few static variables here to manage the LRU cache of
       files */
  //lruCacheSetup(lru_size);
    /* TODO: replace following sample code with code that satisfies the
       requirements of the assignment */

    /* sample code: continually read lines from the client, and send them
       back to the client immediately */
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
	//printf("Waiting for next line on %d\n", connfd);
        while (1) {
            /* read some data; swallow EINTRs */
            if ((nsofar = read(connfd, bufp, nremain)) < 0) {
                if (errno != EINTR)
                    die("read error: ", strerror(errno));
		printf("recieved and EINTR\n");
		continue;
            }
            /* end service to this client on EOF */
            if (nsofar == 0) {
                fprintf(stderr, "received EOF\n");
                return;
            }
            /* update pointer for next bit of reading */
            bufp += nsofar;
            nremain -= nsofar;
            if (!strcmp((bufp- strlen(EOT)), EOT)) {
	      break;
	    }
	    
	}
    bzero((bufp - strlen(EOT)), strlen(EOT));
	//printf("server recieved: %s\n", buf);
	
	//continue;
	char cmd[10][MAXLINE];
	//bzero(cmd, MAXLINE);
	int index = 0;
	int cmdHIndex = 0;
	int cmdVIndex = 0;
	int contentsRead = 0;
    int dynamicRead = 3;
	while(index < nsofar){
	  /* if(cmdVIndex == dynamicRead - 1 && myRequest->checkSum){
            if(cmdHIndex == 32){
              cmdVIndex++;
              index++;
              cmdHIndex = 0;
              cmd[cmdVIndex][cmdHIndex] = '\0';
              continue;
            }
          }else*/
	  if(cmdVIndex < dynamicRead && buf[index] == '\n'){
	    
	    cmd[cmdVIndex][cmdHIndex] = '\0';
	    cmdVIndex++;
	    index++;
	    cmdHIndex = 0;
        if(cmdVIndex == 1){
            check = popType(cmd[0], myRequest);
	    if(myRequest->checkSum) dynamicRead++;
            if(check){ 
	            *bufp = 0;
            }else{
	      printf("Type error %s not valid\n", cmd[0]);
                continue;
            }
        }
	
	continue;
	  }
	  
	  if(cmdVIndex == dynamicRead){
	    contentsRead++;
	  }
	  //cmd[cmdVIndex][cmdHIndex]; remember the alamo
	  cmd[cmdVIndex][cmdHIndex] = buf[index];
	  cmdHIndex++;
	  index++;
	}
       

    if(cmdVIndex < 2){
        //Error
      printf("not enough arguments sent\n");
        continue;
    }

    //request count 0 read cmd
    int temp = 0;
    while(temp <= cmdVIndex){
      //printf("Command number %i: %s\n", temp, cmd[temp]);
      temp++;
    }

    

    //request count 1 get file name
    check = popName(cmd[1], myRequest);
    if(check){
	    *bufp = 0;
	    //printf("name is %s\n", myRequest->name);
    }

    //request count 2 file size 
    if(myRequest->type == PUT){
      check = popSize(cmd[2], myRequest);
      cmd[dynamicRead][myRequest->size_bytes] = '\0';
      //printf("size of the file is: %i\n", myRequest->size_bytes);
        if(check){
            *bufp = 0;
            currentFileContents = malloc(myRequest->size_bytes);
        }
        else{
            *bufp = 0;
	    printf("Size not valid\n");
            continue;
        }
        //myRequest->contents = strdup(cmd[3]);
        makefile(myRequest, cmd[dynamicRead], contentsRead, connfd);
            if(myRequest->checkSum){
	      if(checkSum(cmd[3], myRequest, cmd[4])) response(connfd, "OKC\n");
	      else response(connfd, "DONDE ESTA LOS DROGAS?!?!");
	    } else{
	      response(connfd, "OK\n");
	    }
        
        continue;
    }else{
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
      myRequest->size_bytes = size;
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
	    getBuff->name = strdup(myRequest->name);
	    getBuff->size = myRequest->size_bytes;
	    getBuff->contents = strdup(currentFileContents);
	    addFileBuffer(getBuff);
	    //return the OK to the client w/ the getBuff info
	  }
      char sizeSTR[8192];
      sprintf(sizeSTR, "%s\n%s\n%i\n%s%s", "OK", getBuff->name, getBuff->size, getBuff->contents, EOT);
      response(connfd, sizeSTR);


        }
        else{
          printf("could not open the file: %s", myRequest->name);
          //server needs to send to the client an error message         
        }
	break;
    }
    
    //*bufp = 0; //not sure what this does
    
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
    int  lru_size = 2;
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
    lruCacheSetup(lru_size);
    /* open a socket, and start handling requests */
    int fd = open_server_socket(port);
    handle_requests(fd, file_server, lru_size);

    exit(0);
}

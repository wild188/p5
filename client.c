#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "support.h"

/*
 * help() - Print a help message
 */
void help(char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("Perform a PUT or a GET from a network file server\n");
    printf("  -P    PUT file indicated by parameter\n");
    printf("  -G    GET file indicated by parameter\n");
    printf("  -C    use checksums for PUT and GET\n");
    printf("  -e    use encryption, with public.pem and private.pem\n");
    printf("  -s    server info (IP or hostname)\n");
    printf("  -p    port on which to contact server\n");
    printf("  -S    for GETs, name to use when saving file locally\n");
}

/*
 * die() - print an error and exit the program
 */
void die(const char *msg1, char *msg2) {
    fprintf(stderr, "%s, %s\n", msg1, msg2);
    exit(0);
}

/*
 * connect_to_server() - open a connection to the server specified by the
 *                       parameters
 */
int connect_to_server(char *server, int port) {
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;
    char errbuf[256];                                   /* for errors */

    /* create a socket */
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("Error creating socket: ", strerror(errno));

    /* Fill in the server's IP address and port */
    if ((hp = gethostbyname(server)) == NULL) {
        sprintf(errbuf, "%d", h_errno);
        die("DNS error: DNS error ", errbuf);
    }
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
          (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* connect */
    if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        die("Error connecting: ", strerror(errno));
    return clientfd;
}

void readServerResponse(int fd, ssize_t nsofar, size_t nremain, char *bufp){
  while (1) {
    if ((nsofar = read(fd, bufp, nremain)) < 0) {
      if (errno != EINTR)
	die("read error: ", strerror(errno));
      continue;
    }
    // in echo, server should never EOF                         
    if (nsofar == 0)
      die("Server error: ", "received EOF");
    bufp += nsofar;
    nremain -= nsofar;
    if (*(bufp-1) == '\n') {
      *bufp = 0;
      break;
    }
  }
}

/*void sendRequest(int fd, int type, int step, char *fileName){
  //send GET\n, type = 1 & step = 1
  //send fileName if type = 1 & step = 2
  //send PUT\n, type = 2 & step = 1
  //send fileName if type = 2 & step = 2
  //send size if type = 2 & step = 3
  //send file contents if type = 2 & step = 4
    
   set up a buffer and clear it
  const int MAXLINE = 8192;
  char buf[MAXLINE];
  bzero(buf, MAXLINE); 
  
  //determine the contents of buffer
  if(type == 1 && step == 1){
    strcpy(buf, "GET\n");
  }
  else if(step == 2){
    strcpy(buf, fileName);
    strcat(buf, "\n");
  }
  else if(type == 2 && step == 1){
      strcpy(buf, "PUT\n");
  }
  else if(type == 2 && step == 3){
    off_t size;
    struct stat st;
    if(stat(fileName, &st) == 0){
      size = st.st_size;
	sprintf(buf, "%lu\n", size);
    }
    else{
      printf("Error cannot find file");
      exit(0);
    }
  }
  else if(type == 2 && step == 4){
      //get file contents and copy it to buf
    FILE *in;
    char ch;
    int index = 0;
    in = fopen(fileName, "r");
    if(!in){
      perror("file does not exist");
      exit(0);
    }
    while((ch = fgetc(in)) != EOF){
      buf[index] = ch;
      index++;
    }
    fclose(in);
  }
  
// send keystrokes to the server, handling short counts
  //consider changing strlen
  size_t n = strlen(buf);
  size_t nremain = n;
  ssize_t nsofar;
  char *bufp = buf;
  while (nremain > 0) {
    if ((nsofar = write(fd, bufp, nremain)) <= 0) {
      if (errno != EINTR) {
	fprintf(stderr, "Write error: %s\n", strerror(errno));
	exit(0);
      }
      nsofar = 0;
    }
    nremain -= nsofar;
    bufp += nsofar;
  }
  
}*/

/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
void put_file(int fd, char *put_name) {
  printf("got to the put_file");
  char buf[8192];
  off_t size;
  struct stat st;
  if(stat(put_name, &st) == 0){
    size = st.st_size;
  }
  else{
    printf("Error cannot find file");
    exit(0);
  }
  char temp[size];
  FILE *in;
  char ch;
  int index = 0;
  in = fopen(put_name, "r");
  if(!in){
    perror("file does not exist");
    exit(0);
  }
  while((ch = fgetc(in)) != EOF){
    temp[index] = ch;
    index++;
  }
  fclose(in);
  sprintf(buf, "%s\n%s\n%ul\n%s\n$", "PUT", put_name, size, temp);
  size_t n = strlen(buf);
  size_t nremain = n;
  ssize_t nsofar;
  char *bufp = buf;
  while (nremain > 0) {
    if ((nsofar = write(fd, bufp, nremain)) <= 0) {
      if (errno != EINTR) {
        fprintf(stderr, "Write error: %s\n", strerror(errno));
        exit(0);
      }
      nsofar = 0;
    }
    nremain -= nsofar;
    bufp += nsofar;
  }
  
}

/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
void get_file(int fd, char *get_name, char *save_name) {
  char writeArr[8192];  
  sprintf(writeArr, "%s\n%s\n$", "GET", get_name);
  size_t n = strlen(writeArr);
  size_t nremain1 = n;
  ssize_t nsofar1;
  char *writep = writeArr;
  while (nremain1 > 0) {
    if ((nsofar1 = write(fd, writep, nremain1)) <= 0) {
      if (errno != EINTR) {
	fprintf(stderr, "Write error: %s\n", strerror(errno));
        exit(0);
      }
      nsofar1 = 0;
    }
    nremain1 -= nsofar1;
    writep += nsofar1;
  }


  //billys code
    
    const int MAXLINE = 8192;
    char      buf[MAXLINE];   /* a place to store text from the client */
    bzero(buf, MAXLINE);
      /* read from socket, recognizing that we may get short counts */
    char *bufp = buf;              /* current pointer into buffer */
    ssize_t nremain = MAXLINE;     /* max characters we can still read */
    size_t nsofar;             
	  printf("Waiting for next line on %d\n", fd);
        while (1) {
            /* read some data; swallow EINTRs */
            if ((nsofar = read(fd, bufp, nremain)) < 0) {
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
  char cmd[10][MAXLINE];
	bzero(cmd, MAXLINE);
	int index = 0;
	int cmdHIndex = 0;
	int cmdVIndex = 0;
  int readSoFar = 0;
	while(index < nsofar){
	  if(cmdVIndex < 3 && buf[index] == '\n'){
	    cmd[cmdVIndex][cmdHIndex] = '\0';
        cmdVIndex++;
	    index++;
	    cmdHIndex = 0;
	    continue;
	  }
	  if(cmdVIndex == 3){
      readSoFar++;
    }
	  //cmd[cmdVIndex][cmdHIndex]; remember the alamo
	  cmd[cmdVIndex][cmdHIndex] = buf[index];
	  cmdHIndex++;
	  index++;
	}

  int success = 0;
  if(cmdVIndex >= 3){
    if(!strcmp(cmd[0], "OK")){
      success = 1;
    }
    printf("%s\n", cmd[0]);
  }else{
    printf("No response read.\n");
  }
  int sizeInt;
  if(!sscanf(cmd[2], "%d", &sizeInt)){
      success = 0;
  }

  if(cmdVIndex >= 3 && success){
    FILE *myFile;
    if((myFile = fopen(save_name, "w")) != NULL){
      int i;
      for(i = 0; i < sizeInt; i++){
        fputc(cmd[3][i], myFile);
      }
    }else{
      printf("Failed to open file for write\n");
    }
  }
}
/*
 * main() - parse command line, open a socket, transfer a file
 */
int main(int argc, char **argv) {
    /* for getopt */
    long  opt;
    char *server = NULL;
    char *put_name = NULL;
    char *get_name = NULL;
    int   port;
    char *save_name = NULL;

    check_team(argv[0]);

    /* parse the command-line options. */
    /* TODO: add additional opt flags */
    while ((opt = getopt(argc, argv, "hs:P:G:S:p:")) != -1) {
        switch(opt) {
          case 'h': help(argv[0]); break;
          case 's': server = optarg; break;
          case 'P': put_name = optarg; break;
          case 'G': get_name = optarg; break;
          case 'S': save_name = optarg; break;
          case 'p': port = atoi(optarg); break;
        }
    }

    /* open a connection to the server */
    int fd = connect_to_server(server, port);

    /* put or get, as appropriate */
    if (put_name)
        put_file(fd, put_name);
    else
        get_file(fd, get_name, save_name);

    /* close the socket */
    int rc;
    if ((rc = close(fd)) < 0)
        die("Close error: ", strerror(errno));
    exit(0);
}

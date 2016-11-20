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

/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
void put_file(int fd, char *put_name, int checkSum) {
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

  if(checkSum){
    char cs[256];
    MD5(cs, index, temp);
    sprintf(buf, "%s\n%s\n%ul\n%s\n%s$", "PUTC", put_name, size, cs, temp);
  }else{
    sprintf(buf, "%s\n%s\n%ul\n%s$", "PUT", put_name, size, temp);
  }

  
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

  const int MAXLINE = 8192;
  char      buf1[MAXLINE];   /* a place to store text from the client \
			     */
  bzero(buf1, MAXLINE);
  /* read from socket, recognizing that we may get short counts */
  char *bufp1 = buf1;              /* current pointer into buffer */
  ssize_t nremain1 = MAXLINE;     /* max characters we can still read \
				  */
  size_t nsofar1;
  while (1) {
    /* read some data; swallow EINTRs */
    if ((nsofar1 = read(fd, bufp1, nremain1)) < 0) {
      if (errno != EINTR)
	die("read error: ", strerror(errno));
      printf("recieved and EINTR\n");
      continue;
    }
    /* end service to this client on EOF */
    if (nsofar1 == 0) {
      fprintf(stderr, "received EOF\n");
      return;
    }
    bufp1 += nsofar1;
    nremain1 -= nsofar1;
    if (*(bufp1-1) == '$') {
      break;
    }
  }
  printf("%s", buf1);
  return;
}

/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
void get_file(int fd, char *get_name, char *save_name, int checkSum) {
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
        while (1) {
            /* read some data; swallow EINTRs */
            if ((nsofar = read(fd, bufp, nremain)) < 0) {
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
            if (*(bufp-1) == '\n') {
	              break;
	    }
	   
	    
	}
	
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
	printf("%s\n", cmd[0]);
	int success = 0;
  if(cmdVIndex >= 3){
    if(!strcmp(cmd[0], "OK")){
      success = 1;
    }
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
    int checkSum = 0;

    check_team(argv[0]);

    /* parse the command-line options. */
    /* TODO: add additional opt flags */
    while ((opt = getopt(argc, argv, "hs:P:G:S:p:c:")) != -1) {
        switch(opt) {
          case 'h': help(argv[0]); break;
          case 's': server = optarg; break;
          case 'P': put_name = optarg; break;
          case 'G': get_name = optarg; break;
          case 'S': save_name = optarg; break;
          case 'p': port = atoi(optarg); break;
          case 'c': checkSum = 1; break;
        }
    }

    /* open a connection to the server */
    int fd = connect_to_server(server, port);

    /* put or get, as appropriate */
    if (put_name)
        put_file(fd, put_name, checkSum);
    else
        get_file(fd, get_name, save_name, checkSum);

    /* close the socket */
    int rc;
    if ((rc = close(fd)) < 0)
        die("Close error: ", strerror(errno));
    exit(0);
}

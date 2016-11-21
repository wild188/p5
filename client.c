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

char *EOT = "SUCKA MY BAWLS TWO TIME!";

char *public_key = "-----BEGIN PUBLIC KEY-----\n"
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAwMu7BZF451FjUXYNr323\n"
"aeeaCW2a7s6eHHs8Gz5qgQ/zDegub6is3jwdTZJyGcRcN1DxKQsLcOa3F18KSiCk\n"
"yzIWjNV4YH7GdV7Ke2qLjcQUs7wktGUKyPYJmDWGYv/QN0Sbbol9IbeLjSBHUt16\n"
"xBex5IIpQqDtBy0RZvAMdUUB1rezKka0bC+b5CmE4ysIRFyFiweSlGsSdkaS9q1l\n"
"d+c/V4LMxljNbhdpfpiniWAD3lm9+mDJzToOiqz+nH9SHs4ClEThBAScI00xJH36\n"
"3mDvY0x6HVDyCsueC9jtfZKnI2uwM2tbUU4iDkCaIYm6VE6h1qs5AkrxH1o6K2lC\n"
"kQIDAQAB\n"
  "-----END PUBLIC KEY-----\n";

char *private_key = "-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEowIBAAKCAQEAwMu7BZF451FjUXYNr323aeeaCW2a7s6eHHs8Gz5qgQ/zDegu\n"
"b6is3jwdTZJyGcRcN1DxKQsLcOa3F18KSiCkyzIWjNV4YH7GdV7Ke2qLjcQUs7wk\n"
"tGUKyPYJmDWGYv/QN0Sbbol9IbeLjSBHUt16xBex5IIpQqDtBy0RZvAMdUUB1rez\n"
"Kka0bC+b5CmE4ysIRFyFiweSlGsSdkaS9q1ld+c/V4LMxljNbhdpfpiniWAD3lm9\n"
"+mDJzToOiqz+nH9SHs4ClEThBAScI00xJH363mDvY0x6HVDyCsueC9jtfZKnI2uw\n"
"M2tbUU4iDkCaIYm6VE6h1qs5AkrxH1o6K2lCkQIDAQABAoIBAChqzWNGcu0zb7nF\n"
"IOtYVJocFnvBgYhswlLANwKTHCrAWDjjItD/sHXKbm4ztD3Yn2htTJFJInXhuCJr\n"
"JzIRE9sRPg76NYktKpeybope9LCcmaZwW9WBlTg59Br3pZude14KwPb0Vco6u0Oz\n"
"r6AclD8FpKJ98v5n1Cj79rj4u/PdTXZP+Fmz8y0KAgM1s39rtiaAHKGCcyfb1awf\n"
"pCsYL0IvmU1Z00sPe5dzSqLY4HcIyT2kIMqnC0c0HTPtU6A5GI7dPMQsJy+ZEsWo\n"
"kR4YdymmN3C11gdd8kTpExdm1Ick5GfgUfc5hYYTBUO4n8bWJFJLxY6MtjdRfWHc\n"
"SBg4hw0CgYEA3vYaOzk8gUgUVkhnAsyoRNPkMKOH+EbOWuAssK3lQ2U8gvryJngz\n"
"KaQEGnluHvwK69BkJQQ5+PTMKhvhDZ4Ur9t6K7iZ3io3XLafH+jZk8alxYtZen00\n"
"Z38z2VQ8gjYHjfXGKNs1YGcfb+uJ5a8YMGbNjYTdGkIeWL0DLdrYH78CgYEA3V1R\n"
"fTPCY93kxOfYEnRvsO7HY6/4aESAMthROABd9IbYzmsA2Jkcs9Cns3MvWoLpbUY5\n"
"c36WDn9pZOg8vF0dday9Gr/ZrisEv7MgFl0FloyNsnGviHHFfoLPbOjEPUGXcRy2\n"
"1350nFJ2L0e9XcHgvPSjkmwcLbGgkrtWgjJoMa8CgYB0URPSPcPw9jeV4+PJtBc9\n"
"AQYU0duHjPjus/Dco3vtswzkkCJwK1kVqjlxzlPC2l6gM3FrVk8gMCWq+ixovEWy\n"
"kN+lm4K6Qm/rcGKHdSS9UW7+JfqiSltiexwDj0yZ6bH7P3MHsYShLGtcKhcguj32\n"
"Ukt+PwhSQJgwVzsnWvpRZQKBgQCDFrIdLLufHFZPbOR9+UnzQ1P8asb2KCqq8YMX\n"
"YNBC8GAPzToRCor+yT+mez29oezN81ouVPZT24v0X7sn6RR7DTJnVtl31K3ZQCBu\n"
"XePjRZTb6YsDiCxmQNzJKAaeJ+ug5lo4vwAbWpH2actwbFHEVDNRkIgXXysx+ZK/\n"
"Q06ErQKBgHzXwrSRWppsQGdxSrU1Ynwg0bIirfi2N8zyHgFutQzdkDXY5N0gRG7a\n"
"Xz8GFJecE8Goz8Mw2NigtBC4EystXievCwR3EztDyU5PgvEQV7d+0GLKtCG6QFqC\n"
"gZKlwzSf9rLhfXYCrWgqg7ZXsiaADQePw+fU2dudERxmg3gokBFL\n"
  "-----END RSA PRIVATE KEY-----\n";


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

/*int encryptData(char *fileContents, int size, char* encryptedData){
  //Encrypt with private Key
  //Decrypt with public Key
  char privateKey[]="-----BEGIN RSA PRIVATE KEY-----\n"\
"MIIEowIBAAKCAQEAy8Dbv8prpJ/0kKhlGeJYozo2t60EG8L0561g13R29LvMR5hy\n"\
"vGZlGJpmn65+A4xHXInJYiPuKzrKUnApeLZ+vw1HocOAZtWK0z3r26uA8kQYOKX9\n"\
"Qt/DbCdvsF9wF8gRK0ptx9M6R13NvBxvVQApfc9jB9nTzphOgM4JiEYvlV8FLhg9\n"\
"yZovMYd6Wwf3aoXK891VQxTr/kQYoq1Yp+68i6T4nNq7NWC+UNVjQHxNQMQMzU6l\n"\
"WCX8zyg3yH88OAQkUXIXKfQ+NkvYQ1cxaMoVPpY72+eVthKzpMeyHkBn7ciumk5q\n"\
"gLTEJAfWZpe4f4eFZj/Rc8Y8Jj2IS5kVPjUywQIDAQABAoIBADhg1u1Mv1hAAlX8\n"\
"omz1Gn2f4AAW2aos2cM5UDCNw1SYmj+9SRIkaxjRsE/C4o9sw1oxrg1/z6kajV0e\n"\
"N/t008FdlVKHXAIYWF93JMoVvIpMmT8jft6AN/y3NMpivgt2inmmEJZYNioFJKZG\n"\
"X+/vKYvsVISZm2fw8NfnKvAQK55yu+GRWBZGOeS9K+LbYvOwcrjKhHz66m4bedKd\n"\
"gVAix6NE5iwmjNXktSQlJMCjbtdNXg/xo1/G4kG2p/MO1HLcKfe1N5FgBiXj3Qjl\n"\
"vgvjJZkh1as2KTgaPOBqZaP03738VnYg23ISyvfT/teArVGtxrmFP7939EvJFKpF\n"\
"1wTxuDkCgYEA7t0DR37zt+dEJy+5vm7zSmN97VenwQJFWMiulkHGa0yU3lLasxxu\n"\
"m0oUtndIjenIvSx6t3Y+agK2F3EPbb0AZ5wZ1p1IXs4vktgeQwSSBdqcM8LZFDvZ\n"\
"uPboQnJoRdIkd62XnP5ekIEIBAfOp8v2wFpSfE7nNH2u4CpAXNSF9HsCgYEA2l8D\n"\
"JrDE5m9Kkn+J4l+AdGfeBL1igPF3DnuPoV67BpgiaAgI4h25UJzXiDKKoa706S0D\n"\
"4XB74zOLX11MaGPMIdhlG+SgeQfNoC5lE4ZWXNyESJH1SVgRGT9nBC2vtL6bxCVV\n"\
"WBkTeC5D6c/QXcai6yw6OYyNNdp0uznKURe1xvMCgYBVYYcEjWqMuAvyferFGV+5\n"\
"nWqr5gM+yJMFM2bEqupD/HHSLoeiMm2O8KIKvwSeRYzNohKTdZ7FwgZYxr8fGMoG\n"\
"PxQ1VK9DxCvZL4tRpVaU5Rmknud9hg9DQG6xIbgIDR+f79sb8QjYWmcFGc1SyWOA\n"\
"SkjlykZ2yt4xnqi3BfiD9QKBgGqLgRYXmXp1QoVIBRaWUi55nzHg1XbkWZqPXvz1\n"\
"I3uMLv1jLjJlHk3euKqTPmC05HoApKwSHeA0/gOBmg404xyAYJTDcCidTg6hlF96\n"\
"ZBja3xApZuxqM62F6dV4FQqzFX0WWhWp5n301N33r0qR6FumMKJzmVJ1TA8tmzEF\n"\
"yINRAoGBAJqioYs8rK6eXzA8ywYLjqTLu/yQSLBn/4ta36K8DyCoLNlNxSuox+A5\n"\
"w6z2vEfRVQDq4Hm4vBzjdi3QfYLNkTiTqLcvgWZ+eX44ogXtdTDO7c+GeMKWz4XX\n"\
"uJSUVL5+CVjKLjZEJ6Qc2WZLl94xSwL71E41H4YciVnSCQxVc4Jw\n"\
    "-----END RSA PRIVATE KEY-----\n";
  RSA *tempRSA = createRSA(privateKey, 1);
  int encryptedLength;
  char *encrypted;
  FILE *pubKey;
  int padding = RSA_PKCS1_PADDING;
  //pubKey = fopen("private.pem", "r");
  //tempRSA = PEM_read_RSAPrivateKey(pubKey, &rsa, NULL, NULL);
  // printf("temp RSA is")
  encryptedLength = RSA_private_encrypt(size, fileContents,encryptedData, tempRSA, padding);
  fclose(pubKey);
  return encryptedLength;
}*/
 

/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
int encryptData(char *fileContents, int size, char* encryptedData){
  BIO *bio;
  bio = BIO_new_mem_buf((void*)public_key, (int)strlen(public_key));
  RSA *rsa_publickey = PEM_read_bio_RSA_PUBKEY(bio, NULL, 0, NULL);
  BIO_free(bio);
  bio = BIO_new_mem_buf((void*)private_key, (int)strlen(private_key));
  RSA *rsa_privatekey = PEM_read_bio_RSAPrivateKey(bio, NULL, 0, NULL);
  BIO_free(bio);
  
  int maxSize = RSA_size(rsa_publickey);
  //encryptedData = (unsigned char *) malloc(maxSize * sizeof(char));

  // Fill buffer with encrypted data
  int bytes = RSA_public_encrypt(size, fileContents, encryptedData, rsa_publickey, RSA_PKCS1_PADDING);
  printf("The encryptedData in method is%s\n", encryptedData);
  printf("The size is %i\n", bytes);

  unsigned char *decrypted = (unsigned char *) malloc(1000);

  // Fill buffer with decrypted data
  RSA_private_decrypt(bytes, encryptedData, decrypted, rsa_privatekey, RSA_PKCS1_PADDING);

  printf("decrypted is: %s\n", decrypted);
  return bytes;
}

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
  char *temp = malloc(sizeof(char) * size);
  unsigned char encrypted[8192];
  int encryptedSize = 0;
  
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
    unsigned char cs[256];
    bzero(cs, 256);
    printf("Temp before md5 in hex is:\n%x", temp);
    MD5(temp, index, cs);
    printf("Temp is: %s\ncs is: %x\n", temp, cs);
    sprintf(buf, "%s\n%s\n%i\n%x\n%s%s", "PUTC", put_name, size, cs, temp, EOT);
  }else{
    encryptedSize = encryptData(temp, size, encrypted);
    sprintf(buf, "%s\n%s\n%i\n%s%s", "PUT", put_name, encryptedSize, encrypted, EOT);
  }

  printf("client sending: %s\n", buf);

  
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
    if (!strcmp(bufp - strlen(EOT), EOT)) {
      break;
    }
  }
  bzero((bufp - strlen(EOT)), strlen(EOT));
  printf("%s", buf1);
  return;
}

/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
void get_file(int fd, char *get_name, char *save_name, int checkSum) {
  char writeArr[8192];  
  sprintf(writeArr, "%s\n%s\n%s", "GET", get_name, EOT);
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
            if (!strcmp(bufp - strlen(EOT), EOT)) {
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
  printf("cmd[3] is: %s\n", cmd[3]);
  BIO *bio = BIO_new_mem_buf((void*)private_key, (int)strlen(private_key));
  RSA *rsa_privatekey = PEM_read_bio_RSAPrivateKey(bio, NULL, 0, NULL);
  BIO_free(bio);
  unsigned char *decrypted = (unsigned char *) malloc(1000);

  // Fill buffer with decrypted data                                    
  RSA_private_decrypt(sizeInt, cmd[3], decrypted, rsa_privatekey, RSA_PKCS1_PADDING);

  printf("decrypted is: %s\n", decrypted);
  printf("sizeInt is: %i\n", sizeInt);
  if(cmdVIndex >= 3 && success){
    FILE *myFile;
    if((myFile = fopen(save_name, "w")) != NULL){
      int i;
      for(i = 0; i < sizeInt; i++){
        fputc(decrypted[i], myFile);
      }
    }else{
      printf("Failed to open file for write\n");
    }
  }
}
/*
 * main() - parse command line, open a socket, transfer a file
 */

/*void genKey(){
  int returnVal = 0;
  RSA *rsa = NULL;
  BIGNUM *bNum = NULL;
  BIO *publicKey = NULL, *privateKey = NULL;
  int size = 2048;
  unsigned long x = RSA_F4;

  // 1. generate rsa key
  bNum = BN_new();
  returnVal = BN_set_word(bNum, x);
  // if(ret != 1){
  // goto free_all;
  // }
 
  rsa = RSA_new();
  returnVal = RSA_generate_key_ex(rsa, size, bNum, NULL);
  // if(ret != 1){
  // goto free_all;
  // }
 
  // 2. save public key
  publicKey = BIO_new_file("public.pem", "w+");
  returnVal = PEM_write_bio_RSAPublicKey(publicKey, rsa);
  //if(ret != 1){
  //goto free_all;
  //}
 
  // 3. save private key
  privateKey = BIO_new_file("private.pem", "w+");
  returnVal = PEM_write_bio_RSAPrivateKey(privateKey, rsa, NULL, NULL, 0, NULL, NULL);
 
  // 4. free
  //free_all:
 
  //BIO_free_all(bp_public);
  //BIO_free_all(bp_private);
  //RSA_free(r);
  //BN_free(bne);
 
  return;
  }*/

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
    while ((opt = getopt(argc, argv, "hs:P:G:S:p:c")) != -1) {
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
    //genKey();
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

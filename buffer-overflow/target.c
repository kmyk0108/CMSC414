/* target.c */

/* This program has a buffer overflow vulnerability. */
/* Our task is to exploit this vulnerability, not by
 * modifying this code, but by providing a cleverly
 * constructed input. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

#define STRLEN 1001

ssize_t bof(int sockfd)
{
  struct sockaddr_in addr;
  struct sockaddr* saddr = (struct sockaddr*) &addr;
  char buffer[16];
  char* resp;
  char* str;
  uint16_t echolen;
  socklen_t addr_size = sizeof(addr);
  ssize_t s;
  str = malloc(STRLEN*sizeof(char));
  resp = malloc(STRLEN*sizeof(char));

  bzero(str,STRLEN);
  bzero(buffer,sizeof(buffer));
  s = recvfrom(sockfd, str, STRLEN-1, 0, saddr, &addr_size);
  printf("server received %ld bytes\n",s);
  fflush(stdout); // This clears the output buffer, writing to STDOUT
  sleep(1);
  memcpy(&echolen,str,2);
  echolen = be16toh(echolen);
  memcpy(buffer,str+2,s);
  memcpy(resp,buffer,echolen);
  s = sendto(sockfd, resp, echolen, 0, saddr, addr_size);
  if ( s < 0 ) {
    printf("send failed with error %d\n", errno);
    fflush(stdout);
  }
  return s;
}

ssize_t shim(int sockfd) {
   return bof(sockfd);
}

int main(int argc, char **argv)
{
  int s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(8000);
  bind(s, (struct sockaddr*)&addr, sizeof(addr));

  while(1) {
    ssize_t r = shim(s);
    if ( r < 0 ) {
      break;
    }
    printf("sent %ld bytes\n", r);
  }

  printf("Returned Properly\n");
  return 1;
}

/* talker.c - a datagram 'client'
 * need to supply host name/IP and one word message,
 * e.g. talker localhost hello
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>         /* for gethostbyname() */

#define PORT 123     /* server port the client connects to */

int main( int argc, char * argv[]) {
  int sockfd, numbytes;
  static unsigned char buf[48];
  struct hostent *he;
  struct sockaddr_in their_addr;    /* server address info */

  buf[0] = 0x03;
  if( argc != 2) {
    fprintf( stderr, "usage: talker hostname message\n");
    exit( 1);
  }
  /* resolve server host name or IP address */
  if( (he = gethostbyname( argv[ 1])) == NULL) {
    perror( "Talker gethostbyname");
    exit( 1);
  }

  if( (sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror( "Talker socket");
    exit( 1);
  }

  memset( &their_addr,0, sizeof(their_addr)); /* zero struct */
  their_addr.sin_family = AF_INET;    /* host byte order .. */
  their_addr.sin_port = htons( PORT); /* .. short, netwk byte order */
  their_addr.sin_addr = *((struct in_addr *)he -> h_addr);

  if( (numbytes = sendto( sockfd, buf, 48, 0,
      (struct sockaddr *)&their_addr, sizeof( struct sockaddr))) == -1) {
      perror( "Talker sendto");
      exit( 1);
  }

  printf( "Sent %d bytes to %s\n", numbytes,
                          inet_ntoa( their_addr.sin_addr));

  close( sockfd);
  return 0;
}

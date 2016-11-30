/* listener.c - a datagram socket 'server'
 * simply displays message received then dies!
 */

#include "sntpserver.h"

void get_requests(struct connection_info *cn);
int initialise_server(struct connection_info *cn);

/*
  TODO:
    - add exit key
*/



int main( void) {
  struct connection_info my_server;

  initialise_server(&my_server);

  while(1){
    get_requests(&my_server);
  }

  close_connection(my_server);
  return 0;
}


void get_requests(struct connection_info *cn){
  int addr_len, numbytes;
  char buf[ MAXBUFLEN];
  struct sockaddr_in their_addr;   /* client's address info */

  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( cn->sockfd, buf, MAXBUFLEN - 1, 0,
                (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror( "Listener recvfrom");
      exit( 1);
  }

  printf( "Got packet from %s\n", inet_ntoa( their_addr.sin_addr));
  printf( "Packet is %d bytes long\n", numbytes);
  buf[ numbytes] = '\0';  /* end of string */
  printf( "Packet contains \"%s\"\n", buf);
}


int initialise_server(struct connection_info *cn){
  if( (cn->sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
      fprintf( stderr, "ERROR: cant create a socket");
      return 1;
  }

  memset( &cn->addr, 0, sizeof( cn->addr));    /* zero struct */
  cn->addr.sin_family = AF_INET;              /* host byte order ... */
  cn->addr.sin_port = htons( MYPORT); /* ... short, network byte order */
  cn->addr.sin_addr.s_addr = INADDR_ANY;

  if( bind( cn->sockfd, (struct sockaddr *)&cn->addr,
                      sizeof( struct sockaddr)) == -1) {
       fprintf( stderr, "ERROR: cant bind to socket\n");
       return 1;
  }
  return 0;
}

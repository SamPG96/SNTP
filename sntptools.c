#include "sntptools.h"

void close_connection(struct connection_info cn){
  close( cn.sockfd);
}


int send_SNTP_packet(struct ntp_packet *pkt, int sockfd, struct sockaddr_in addr){
  int numbytes;
  if( (numbytes = sendto( sockfd, pkt, 48, 0, //48 TODO: make sizeof pkt work
      (struct sockaddr *)&addr, sizeof( struct sockaddr))) == -1) {
    fprintf( stderr, "WARNING: error with sending packet\n");
    return  1;
  }
  printf( "INFO: Sent %d bytes to %s\n", numbytes,
                          inet_ntoa( addr.sin_addr));
  return 0;
}

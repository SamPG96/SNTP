#include "sntptools.h"


struct ntp_time_t get_ntp_time_of_day(){
  struct ntp_time_t ts_ntp;
  struct timeval ts_unix;

  gettimeofday(&ts_unix, NULL);
  convert_unix_time_into_ntp_time(&ts_unix, &ts_ntp);
  return ts_ntp;
}


int recieve_SNTP_packet(int sockfd, struct ntp_packet *pkt,
                        struct sockaddr_in *addr, struct timeval *dest_time,
                        int debug){
  socklen_t addr_len;
  int numbytes;

  memset( pkt, 0, sizeof *pkt );
  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( sockfd, pkt, MAXBUFLEN - 1, 0,
               (struct sockaddr *)addr, &addr_len)) == -1) {
    print_debug(debug, "socket recv timeout");
    return  1;
  }
  gettimeofday(dest_time, NULL); // store time of packet arrival
  print_debug(debug, "got packet from %s", inet_ntoa( addr->sin_addr));
  return 0;
}


int send_SNTP_packet(struct ntp_packet *pkt, int sockfd, struct sockaddr_in addr,
                     int debug){
  int numbytes;
  if( (numbytes = sendto( sockfd, pkt, sizeof(struct ntp_packet), 0,
      (struct sockaddr *)&addr, sizeof( struct sockaddr))) == -1) {
    print_debug(debug, "error with sending packet");
    return  1;
  }
  print_debug(debug,"sent %d bytes to %s", numbytes,
                          inet_ntoa( addr.sin_addr));
  return 0;
}

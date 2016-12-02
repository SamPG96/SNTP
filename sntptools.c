#include "sntptools.h"

void close_udp_socket(struct host_info cn){
  close( cn.sockfd);
}


struct ntp_time_t get_ntp_time_of_day(){
  struct ntp_time_t ts_ntp;
  struct timeval ts_unix;

  gettimeofday(&ts_unix, NULL);
  convert_unix_time_into_ntp_time(&ts_unix, &ts_ntp);
  return ts_ntp;
}


int send_SNTP_packet(struct ntp_packet *pkt, int sockfd, struct sockaddr_in addr,
                     int debug_enabled){
  int numbytes;
  if( (numbytes = sendto( sockfd, pkt, 48, 0, //48 TODO: make sizeof pkt work
      (struct sockaddr *)&addr, sizeof( struct sockaddr))) == -1) {
    print_debug(debug_enabled, "error with sending packet");
    return  1;
  }
  print_debug(debug_enabled,"sent %d bytes to %s", numbytes,
                          inet_ntoa( addr.sin_addr));
  return 0;
}

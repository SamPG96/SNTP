/* listener.c - a datagram socket 'server'
 * simply displays message received then dies!
 */

#include "sntpserver.h"

struct sntp_request{
  struct host_info client;
  struct ntp_packet pkt;
  struct ntp_time_t time_of_request;
};

struct ntp_packet create_reply_packet(struct sntp_request *c_req);
int initialise_server(int *sockfd, struct host_info *cn);
int setup_multicast(int sockfd, struct host_info cn);

/*
  TODO:
    - add exit key
    - add config file
    - add getopt
    - check request packet
    - add supressed output
*/

int main( void) {
  int sockfd;
  struct host_info my_server;
  struct sntp_request client_req;
  struct ntp_packet reply_pkt;
  struct timeval request_t_unix;

  initialise_server(&sockfd, &my_server);
  setup_multicast(sockfd, my_server);

  while(1){
    // TODO: set to debug config option
    if (recieve_SNTP_packet(sockfd, &client_req.pkt, &client_req.client.addr,
                            &request_t_unix, 1) != 0){
      fprintf(stderr, "ERROR: while listening");
      continue;
    }

    convert_unix_time_into_ntp_time(&request_t_unix, &client_req.time_of_request);

    reply_pkt = create_reply_packet(&client_req);
    //TODO: replace 1 with debug enabled variable
    if (send_SNTP_packet(&reply_pkt, sockfd, client_req.client.addr, 1) == 0){
      printf("succesffuly sent reply packet\n");
    }
  }

  close(sockfd);
  return 0;
}


//void check_request(struct sntp_request *c_req){

//}

struct ntp_packet create_reply_packet(struct sntp_request *c_req){
  int req_version;
  struct ntp_time_t transmit_ts_ntp;
  struct ntp_packet reply_pkt;

  // get request version
  req_version = (c_req->pkt.li_vn_mode >> 3) & 0x7;
  // set version to the same as the client version and mode to 4(server)
  reply_pkt.li_vn_mode = (req_version << 3) | 4; // (vn << 3) | mode
  // TODO: check this
  reply_pkt.stratum = 1;
  // copy poll from request
  reply_pkt.poll = c_req->pkt.poll;
  // TODO: precision, reference timestamp/ref

  reply_pkt.originate_timestamp.second = c_req->pkt.transmit_timestamp.second;
  reply_pkt.originate_timestamp.fraction = c_req->pkt.transmit_timestamp.fraction;

  // add recieve time
  reply_pkt.receive_timestamp.second = htonl(c_req->time_of_request.second);
  reply_pkt.receive_timestamp.fraction = htonl(c_req->time_of_request.fraction);

  // add transmit time
  transmit_ts_ntp = get_ntp_time_of_day();
  reply_pkt.transmit_timestamp.second = htonl(transmit_ts_ntp.second);
  reply_pkt.transmit_timestamp.fraction = htonl(transmit_ts_ntp.fraction);
  return reply_pkt;
}


int setup_multicast(int sockfd, struct host_info cn){
  struct ip_mreq multi_req;

  multi_req.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDRESS);
  multi_req.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multi_req,
                sizeof(multi_req)) < 0) {
    perror("setsockopt for multi membership");
    return 1;
  }
  return 0;
}


int initialise_server(int *sockfd, struct host_info *cn){
  int optval;

  if( (*sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
      fprintf( stderr, "ERROR: cant create a socket");
      return 1;
  }

  optval = 1;
  if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
     fprintf( stderr, "ERROR: Reusing address failed");
     return 1;
  }

  memset( &cn->addr, 0, sizeof( cn->addr));    /* zero struct */
  cn->addr.sin_family = AF_INET;              /* host byte order ... */
  cn->addr.sin_port = htons( MYPORT); /* ... short, network byte order */
  cn->addr.sin_addr.s_addr = INADDR_ANY;

  if( bind( *sockfd, (struct sockaddr *)&cn->addr,
                      sizeof( struct sockaddr)) == -1) {
       fprintf( stderr, "ERROR: cant bind to socket\n");
       return 1;
  }
  return 0;
}

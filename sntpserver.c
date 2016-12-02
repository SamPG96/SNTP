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
void get_a_request(struct host_info cn, struct sntp_request *c_req);
int initialise_server(struct host_info *cn);
int send_SNTP_packet(struct ntp_packet *pkt, int sockfd, struct sockaddr_in addr);

/*
  TODO:
    - add exit key
    - add config file
    - add getopt
    - check request packet
*/

int main( void) {
  struct host_info my_server;
  struct sntp_request client_req;
  struct ntp_packet reply_pkt;

  initialise_server(&my_server);

  while(1){
    get_a_request(my_server, &client_req);
    reply_pkt = create_reply_packet(&client_req);
    send_SNTP_packet(&reply_pkt, my_server.sockfd, client_req.client.addr);
  }

  close_udp_socket(my_server);
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


void get_a_request(struct host_info cn, struct sntp_request *c_req){
  int addr_len;
  int numbytes;

  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( cn.sockfd, &c_req->pkt, MAXBUFLEN - 1, 0,
                (struct sockaddr *)&c_req->client.addr, &addr_len)) == -1) {
      fprintf( stderr, "ERROR: listening on socker");
      exit( 1);
  }
  // store time of request
  c_req->time_of_request = get_ntp_time_of_day();

  printf( "Got packet from %s\n", inet_ntoa( c_req->client.addr.sin_addr));
  printf( "Packet is %d bytes long\n", numbytes);
}


int initialise_server(struct host_info *cn){
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

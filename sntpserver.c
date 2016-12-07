/* listener.c - a datagram socket 'server'
 * simply displays message received then dies!
 */

#include "sntpserver.h"


/*
  TODO:
    - add getopt
    - check request packet
*/

int main( int argc, char * argv[]) {
  int sockfd;
  struct host_info my_server;
  struct sntp_request client_req;
  struct ntp_packet reply_pkt;
  struct timeval request_t_unix;
  struct server_settings s_set;

  s_set = get_server_settings(argc, argv);
  if (initialise_server(&sockfd, s_set.server_port, &my_server, s_set.debug) != 0){
    fprintf(stderr, "error initialising server\n");
    exit(1);
  }
  if(s_set.manycast_enabled){
    if(setup_manycast(sockfd, s_set.manycast_address, s_set.debug) != 0){
      fprintf(stderr, "error setting up socket for manycast\n");
      exit(1);
    }
  }

  while(1){
    if (recieve_SNTP_packet(sockfd, &client_req.pkt, &client_req.client.addr,
                            &request_t_unix, s_set.debug) != 0){
      fprintf(stderr, "error while listening for requests\n");
      continue;
    }

    printf("recieved a packet from %s\n",
           inet_ntoa(client_req.client.addr.sin_addr));
    convert_unix_time_into_ntp_time(&request_t_unix, &client_req.time_of_request);

    reply_pkt = create_reply_packet(&client_req);
    if (send_SNTP_packet(&reply_pkt, sockfd, client_req.client.addr,
                         s_set.debug) == 0){
      printf("succesffuly sent reply packet to %s\n",
             inet_ntoa(client_req.client.addr.sin_addr));
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


struct server_settings get_server_settings(int argc, char * argv[]){
  struct server_settings s_set;

  // set relevant settings to their defaults
  s_set.server_port = DEFAULT_SERVER_PORT;
  s_set.debug = DEFAULT_debug;
  s_set.manycast_enabled = DEFAULT_MANYCAST_ENABLED;
  s_set.manycast_address = DEFAULT_MANYCAST_ADDRESS;

  // dont parse config file if it doesnt exist
  if (0 == access(CONFIG_FILE, 0)){
    // update settings from options defined in the config file
    parse_config_file(&s_set);
  }
  else{
    print_debug(s_set.debug, "no config file found for '%s'", CONFIG_FILE);
  }

  return s_set;
 }


int initialise_server(int *sockfd, int port, struct host_info *cn, int debug){
  int optval;

  if( (*sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
      print_debug(debug, "error creating a socket");
      return 1;
  }

  optval = 1;
  if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
     print_debug( debug, "error setting up reuseable address");
     return 1;
  }

  memset( &cn->addr, 0, sizeof( cn->addr));    /* zero struct */
  cn->addr.sin_family = AF_INET;              /* host byte order ... */
  cn->addr.sin_port = htons( port); /* ... short, network byte order */
  cn->addr.sin_addr.s_addr = INADDR_ANY;

  if( bind( *sockfd, (struct sockaddr *)&cn->addr,
                      sizeof( struct sockaddr)) == -1) {
       print_debug( debug,  "error binding to socket");
       return 1;
  }
  return 0;
}


void parse_config_file(struct server_settings *s_set){
  int port;
  int debug;
  int manycast_enabled;
  const char *manycast_address;
  config_t cfg;

  //TODO: Remove if statements

  cfg = setup_config_file(CONFIG_FILE); // get config file options

  if (config_lookup_bool(&cfg, "manycast_enabled", &manycast_enabled)){
    s_set->manycast_enabled = manycast_enabled;
  }

  // set the manycast address if manycast is enabled via the commandline
  if (s_set->manycast_enabled){
      if (config_lookup_string(&cfg, "manycast_address", &manycast_address)){
        s_set->manycast_address = manycast_address;
      }
  }

  if (config_lookup_int(&cfg, "server_port", &port)){
    s_set->server_port = port;
  }

  if (config_lookup_bool(&cfg, "debug", &debug)){
    s_set->debug = debug;
  }
}


int setup_manycast(int sockfd, const char *manycast_address, int debug){
  struct ip_mreq multi_req;

  multi_req.imr_multiaddr.s_addr = inet_addr(manycast_address);
  multi_req.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multi_req,
                sizeof(multi_req)) < 0) {
    print_debug(debug, "unable to setup socket for manycast");
    return 1;
  }
  return 0;
}

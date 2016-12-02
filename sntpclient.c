/* talker.c - a datagram 'client'
 * need to supply host name/IP and one word message,
 * e.g. talker localhost hello
 *
 */
#include "sntpclient.h"

/* TODO:
    - add option to supress output
    - use getopt
    - tidy config
    - handle when no config file is present
    - repeat requests (no less than 1 minute, enforce gap in main)
    - handle kiss-o-death,check client operations in RFC
    - CHECK: sanity check: check recieve time is non-zero?
    - CHECK: print to stderr, diff between perror??
    - Add include guards
    - add readme
*/





int main( int argc, char * argv[]) {
  int exit_code;
  int counter;
  int s_counter; // number of successful requests
  double offset;
  double offset_total; // used for average calculation
  double offset_avg;
  double error_bound;
  double error_bound_total; // used for average calculation
  double error_bound_avg;
  struct client_settings c_settings;

  // check cmd line arguments
  if (process_cmdline(argc, argv) != 0){
    exit(1);
  }

  c_settings = get_client_settings(argc, argv);

  // only get the time once if timed repeats updates is disabled
  if (c_settings.timed_repeat_updates_enabled !=1 ){
    if ((exit_code = unicast_mode(c_settings, &offset, &error_bound)) != 0){
      print_unicast_error(exit_code);
    }
  }
  else{
    // request the time from the server timed_repeat_updates_limit amount of times
    for (counter = 0; counter < c_settings.timed_repeat_updates_limit; counter++){
      if ((exit_code = unicast_mode(c_settings, &offset, &error_bound)) != 0){
        print_unicast_error(exit_code);
      }
      offset_total += offset;
      error_bound_total += error_bound;
      s_counter++; // keep track of succesful requests
    }
    offset_avg = offset_total / s_counter;
    error_bound_avg = error_bound_total / s_counter;
    printf("\nStatistics -> offset average: %f, error bound average: +/- %f\n",
            offset_avg, error_bound_avg);
  }
  return 0;
}

/*
  Return codes:
    0 - success
    1 - other error
    2 - host doesnt exist
    3 - cant create socket
    4 - max retry's hit
*/
int unicast_mode(struct client_settings c_settings, double *offset,
                 double *error_bound){
  struct timeval time_of_prev_request;
  int exit_code;
  int rem_time;
  int retry_count;
  int valid_reply;
  struct host_info server;
  struct ntp_packet request_pkt; // request from client to server
  struct ntp_packet reply_pkt; // reply from server to client
  struct core_ts serv_ts; // core times

  // connect to ntp server
  if ((exit_code = initialise_udp_transfer(c_settings, &server)) != 0){
    return exit_code;
  }

  retry_count = 1;
  valid_reply = 0;
  // enforces the loop below to skip the first wait check, as no timer has been
  // set yet.
  time_of_prev_request.tv_sec = -1;

  // keep retrying until a valid packet has been identified.
  while (!valid_reply){
    if (retry_count > c_settings.max_unicast_retries){
      // stop trying and return an error
      return 4;
    }

    /* enforce a min wait between requests, to avoid getting blocked by ther server.
       note that the time between requests will be the value of recv_timeout if it
       is larger number than poll_wait, as the timer starts before the
       request is sent.
    */
    if (time_of_prev_request.tv_sec != -1 &&
            get_elapsed_time(time_of_prev_request) <= c_settings.poll_wait){
      continue;
    }

    // build sntp request packet
    create_packet(&request_pkt);

    // start timer
    time_of_prev_request = start_timer();

    // send request packet to server
    if (send_SNTP_packet(&request_pkt, server.sockfd, server.addr) != 0){
      rem_time = c_settings.poll_wait - get_elapsed_time(time_of_prev_request);
      printf("WARNING: error sending request packet, polling again in %i second(s).\n",
              (rem_time<0)?0:rem_time); // stops rem_time appearing below zero
      retry_count++;
      continue;
    }

    // recieve NTP response packet from server
    if (recieve_SNTP_packet(&reply_pkt, server, &serv_ts) != 0){
      rem_time = c_settings.poll_wait - get_elapsed_time(time_of_prev_request);
      printf("WARNING: error receiving reply packet, polling again in %i second(s).\n",
             (rem_time<0)?0:rem_time);
      retry_count++;
      continue;
    }

    // check reply packet is valid and trusted
    if (run_sanity_checks(request_pkt, reply_pkt) != 0){
      rem_time = c_settings.poll_wait - get_elapsed_time(time_of_prev_request);
      printf("WARNING: error running sanity checks, polling again in %i second(s).\n",
             (rem_time<0)?0:rem_time);
      retry_count++;
      continue;
    }
    valid_reply = 1;
  }

  get_timestamps_from_packet_in_epoch_time(&reply_pkt, &serv_ts);
  *offset = calculate_clock_offset(serv_ts);
  *error_bound = calculate_error_bound(serv_ts);
  print_server_results(serv_ts.transmit_timestamp, *offset, *error_bound,
                              server, reply_pkt.stratum);

  close_udp_socket(server);
  return 0;
}


double calculate_clock_offset(struct core_ts ts){
  double t1 = ts.originate_timestamp.tv_sec + ((double) ts.originate_timestamp.tv_usec / 1000000);
  double t2 = ts.receive_timestamp.tv_sec + ((double) ts.receive_timestamp.tv_usec / 1000000);
  double t3 = ts.transmit_timestamp.tv_sec + ((double) ts.transmit_timestamp.tv_usec / 1000000);
  double t4 = ts.destination_timestamp.tv_sec + ((double) ts.destination_timestamp.tv_usec / 1000000);

  return ((t2 - t1) + (t3 - t4)) / 2;
}


double calculate_error_bound(struct core_ts ts){
  double t1 = ts.originate_timestamp.tv_sec + ((double) ts.originate_timestamp.tv_usec / 1000000);
  double t2 = ts.receive_timestamp.tv_sec + ((double) ts.receive_timestamp.tv_usec / 1000000);
  double t3 = ts.transmit_timestamp.tv_sec + ((double) ts.transmit_timestamp.tv_usec / 1000000);
  double t4 = ts.destination_timestamp.tv_sec + ((double) ts.destination_timestamp.tv_usec / 1000000);

  return (t4 - t1) - (t3 - t2);
}


char * convert_epoch_time_to_human_readable(struct timeval epoch_time){
   char *readable_time_string;
   struct tm  ts;
   char time_convertion[35];

   readable_time_string = (char*)malloc(35);
   // TODO: understand more
   ts = *localtime(&epoch_time.tv_sec);
   strftime(time_convertion, sizeof(time_convertion), "%Y-%m-%d %H:%M:%S", &ts);
   sprintf(readable_time_string, "%s.%li", time_convertion, epoch_time.tv_usec);
   return (char *)readable_time_string;
 }


void create_packet(struct ntp_packet *pkt){
  struct ntp_time_t transmit_ts_ntp;

  memset( pkt, 0, sizeof *pkt );
   // set SNTP V4 and Mode 3(client)
  pkt->li_vn_mode = (4 << 3) | 3; // (vn << 3) | mode
  transmit_ts_ntp = get_ntp_time_of_day();
  pkt->transmit_timestamp.second =  htonl(transmit_ts_ntp.second);
  pkt->transmit_timestamp.fraction = htonl(transmit_ts_ntp.fraction);
 }


/*
  precedence order(from high to low):
    - commandline
    - config file
    - defaults
*/
struct client_settings get_client_settings(int argc, char * argv[]){
   int max_unicast_retries;
   int repeats_enabled;
   int timed_repeat_updates_limit;
   int port;
   int poll_wait;
   int recv_timeout;
   struct client_settings c_settings;
   config_t cfg;

   cfg = setup_config_file(CONFIG_FILE); // get config file options

   // host should always come from the commandline
   c_settings.server_host = argv[1];

   // set server port
   if (config_lookup_int(&cfg, "server_port", &port)){
     c_settings.server_port = port;
   }
   else{
     c_settings.server_port = DEFAULT_SERVER_PORT;
   }

   // set socket timeout
   if (config_lookup_int(&cfg, "recv_timeout", &recv_timeout)){
     c_settings.recv_timeout = recv_timeout;
   }
   else{
     c_settings.recv_timeout = DEFAULT_RECV_TIMEOUT;
   }

   // set max unicast retry limit
   if (config_lookup_int(&cfg, "max_unicast_retries", &max_unicast_retries)){
     c_settings.max_unicast_retries = max_unicast_retries;
   }
   else{
     c_settings.max_unicast_retries = DEFAULT_MAX_UNICAST_RETRY_LIMIT;
   }

   // set minimum time till polling the same server again
   if (config_lookup_int(&cfg, "poll_wait", &poll_wait)){
     c_settings.poll_wait = poll_wait;
   }
   else{
     c_settings.poll_wait = DEFAULT_MIN_POLL_WAIT;
   }

   // only store the number of repeats if timed repeats are enabled
   if (config_lookup_bool(&cfg, "timed_repeat_updates_enabled", &repeats_enabled)){
     c_settings.timed_repeat_updates_enabled = repeats_enabled;
     if (repeats_enabled == 1){
       // the maximum number of times to fetch the server time
       if (config_lookup_int(&cfg, "timed_repeat_updates_limit", &timed_repeat_updates_limit)){
         c_settings.timed_repeat_updates_limit = timed_repeat_updates_limit;
       }
       else{
         c_settings.timed_repeat_updates_limit = DEFAULT_REPEAT_UPDATE_LIMIT;
       }
     }
   }
   else{
     c_settings.timed_repeat_updates_enabled = DEFAULT_REPEAT_UPDATES_ENABLED;
   }

   return c_settings;
 }


int get_elapsed_time(struct timeval start_time){
   struct timeval end_time;

   gettimeofday(&end_time, NULL);
   return end_time.tv_sec - start_time.tv_sec;
 }


void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt,
                                              struct core_ts *ts ){
  struct ntp_time_t originate_timestamp_ntp;
  struct ntp_time_t receive_timestamp_ntp;
  struct ntp_time_t transmit_timestamp_ntp;

  //TODO: create another struct like ntp_time to hold unorderd timestamps
  originate_timestamp_ntp.second = ntohl(pkt->originate_timestamp.second);
  originate_timestamp_ntp.fraction = ntohl(pkt->originate_timestamp.fraction);
  convert_ntp_time_into_unix_time(&originate_timestamp_ntp, &ts->originate_timestamp);

  receive_timestamp_ntp.second = ntohl(pkt->receive_timestamp.second);
  receive_timestamp_ntp.fraction = ntohl(pkt->receive_timestamp.fraction);
  convert_ntp_time_into_unix_time(&receive_timestamp_ntp, &ts->receive_timestamp);

  transmit_timestamp_ntp.second = ntohl(pkt->transmit_timestamp.second);
  transmit_timestamp_ntp.fraction = ntohl(pkt->transmit_timestamp.fraction);
  convert_ntp_time_into_unix_time(&transmit_timestamp_ntp, &ts->transmit_timestamp);
}


int initialise_udp_transfer(struct client_settings c_settings,
                                    struct host_info *cn){
  struct hostent *he;
  struct sockaddr_in their_addr;    /* server address info */
  struct in_addr ipaddr;

  if (inet_pton(AF_INET, c_settings.server_host, &ipaddr) != 0){
    he = gethostbyaddr(&ipaddr, sizeof(ipaddr),AF_INET);
    cn->name = he->h_name;
  }
  else{
    // assume address is a hostname, resolve server host name
    if( (he = gethostbyname( c_settings.server_host)) == NULL) {
      fprintf( stderr, "ERROR: initialise_udp_transfer: host not found\n");
      return 2;
    }
    cn->name = c_settings.server_host;
  }

  if( (cn->sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
    fprintf( stderr, "ERROR: initialise_udp_transfer: error creating socket\n");
    return 3;
  }

  set_socket_recvfrom_timeout(cn->sockfd, c_settings.recv_timeout);

  memset( &their_addr,0, sizeof their_addr); /* zero struct */
  their_addr.sin_family = AF_INET;    /* host byte order .. */
  their_addr.sin_port = htons( c_settings.server_port); /* .. short, netwk byte order */
  their_addr.sin_addr = *((struct in_addr *)he -> h_addr);

  cn->addr = their_addr;
  return 0;
 }


void print_server_results(struct timeval transmit_time, double offset,
                          double error_bound, struct host_info cn,
                          int stratum){
  char *time_str;

  time_str = convert_epoch_time_to_human_readable(transmit_time);
  // TODO: add timezone for (+0000)

  // output timestamp, clock offset and errorbound
  printf("%s (+0000) %f +/- %f ", time_str, offset, error_bound);
  // print hostname of the server if one exists
  if (cn.name){
    printf("%s ", cn.name);
  }
  // output ip address, stratum and no-leap
  printf("%s s%i no-leap\n", inet_ntoa( cn.addr.sin_addr), stratum);
}


void print_unicast_error(int error_code){
  printf("error sending unicast request - ");
  switch(error_code){
    case 2:
      printf("unicast server not found\n");
      break;
    case 3:
      printf("failed to create socket to server\n");
      break;
    case 4:
      printf("max number of retries hit\n");
      break;
    default:
      printf("unknown(code=%i)\n", error_code);
  }
}


int process_cmdline(int argc, char * argv[]){
    if( argc != 2) {
      fprintf( stderr, "usage: %s hostname\n", argv[0]);
      return 1;
    }
    return 0;
  }


int recieve_SNTP_packet(struct ntp_packet *pkt, struct host_info cn,
                        struct core_ts *ts){
  int addr_len;
  int numbytes;

  memset( pkt, 0, sizeof *pkt );
  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( cn.sockfd, pkt, MAXBUFLEN - 1, 0,
               (struct sockaddr *)&cn.addr, &addr_len)) == -1) {
    fprintf( stderr, "WARNING: timeout while waiting for server reply\n");
    return  1;
  }
  gettimeofday(&ts->destination_timestamp, NULL);
  printf( "INFO: Got packet from %s\n", inet_ntoa( cn.addr.sin_addr));
  printf( "INFO: Recieved packet is %d bytes long\n\n", numbytes);
  return 0;
}


int run_sanity_checks(struct ntp_packet req_pkt, struct ntp_packet rep_pkt){
  int rep_mode;
  int rep_version;
  int req_version;

  rep_mode = rep_pkt.li_vn_mode & 0x7; // extract first 3 bits
  req_version = (req_pkt.li_vn_mode >> 3) & 0x7; // extract bits 3 to 5
  rep_version = (rep_pkt.li_vn_mode >> 3) & 0x7; // extract bits 3 to 5

  // the originate time in the server reply should be the same as the transmit
  // time in the request
  if ((req_pkt.transmit_timestamp.second != rep_pkt.originate_timestamp.second) ||
        (req_pkt.transmit_timestamp.fraction != rep_pkt.originate_timestamp.fraction)){
    fprintf( stderr, "WARNING: the originate time in the server reply does not "
             "match the transmit time in the request.\n");
    return 1;
  }

  // check stratum is in range
  else if (rep_pkt.stratum <= 0 || rep_pkt.stratum > 15){
    fprintf( stderr, "WARNING: stratum is not in range 1 to 15(stratum=%i)\n", rep_pkt.stratum);
    return 1;
  }

  // transmit time in the reply packet cant be zero
  else if (rep_pkt.transmit_timestamp.second == 0 &&
                rep_pkt.transmit_timestamp.fraction ==0){
    fprintf( stderr, "WARNING: transmit time of reply packet is zero");
    return 1;
  }

  // check mode is 4(server)
  else if (rep_mode != 4){
    fprintf( stderr, "WARNING: mode of reply packet is not server(mode=%i)\n", rep_mode);
    return 1;
  }

  // server must be the same version as the client. This check irradicates
  // the need to check if the version is non-zero as the client can never
  // be non-zero.
  else if (req_version != rep_version){
    fprintf( stderr, "WARNING: server should be of the same version as the client\n");
    return 1;
  }

  // TODO: fix this, infinity is defined as 1( stated in the RFC)
/*  else if (rep_pkt.root_delay >= 0 && rep_pkt.root_delay < 1){
    fprintf( stderr, "WARNING: root_delay of reply should be >=0 and < 1(root_delay=%i)", rep_pkt.root_delay);
    return 1;
  }

  printf("%f\n", (double)ntohl(rep_pkt.root_delay) / 1000000);*/
  return 0;


}


struct timeval start_timer(){
  struct timeval start_time;

  gettimeofday(&start_time, NULL);
  return start_time;
}

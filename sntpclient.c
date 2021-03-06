/* sntpclient.c - used to query a server for the time
 *
 */
#include "sntpclient.h"


int main( int argc, char * argv[]) {
  int exit_code;
  int counter;
  int s_counter; // number of successful requests
  int num_available_servers;
  double offset;
  double offset_total; // used for average calculation
  double offset_avg;
  double error_bound;
  double error_bound_total; // used for average calculation
  double error_bound_avg;
  char *ntp_servers[MANYCAST_MAX_SERVERS]; // addresses of available ntp servers
  struct client_settings c_set;
  struct timeval poll_timer; // tracks time next next poll

  s_counter = 0;
  offset_avg = 0;
  error_bound_avg = 0;

  c_set = get_client_settings(argc, argv);

  if (c_set.manycast_enabled){
    exit_code = discover_unicast_servers_with_manycast(&c_set, ntp_servers,
                                                      &num_available_servers);
    if (exit_code == 0){
      // use the first server that replied for further unicast operations
      c_set.server_host = ntp_servers[0];
      print_debug(c_set.debug, "using server '%s' for further unicast "
                                       "operations", c_set.server_host);
    }
    else{
      print_error_message(exit_code);
      exit(1);
    }
  }

  // only get the time once if timed repeat updates is disabled
  if (c_set.timed_repeat_updates_enabled !=1 ){
    if ((exit_code = unicast_mode(c_set, &offset, &error_bound,
                                  &poll_timer)) != 0){
      print_error_message(exit_code);
    }
  }
  else{
    // request the time from the server timed_repeat_updates_limit amount of times
    for (counter = 0; counter < c_set.timed_repeat_updates_limit; counter++){
      // wait the minimum amount of time to poll again for the next time sample,
      // unless this is the first request.
      // note that the timer is started in the function unicast_mode when a
      // request is sent
      while (counter != 0 && get_elapsed_time(poll_timer) <= c_set.poll_wait);

      if ((exit_code = unicast_mode(c_set, &offset, &error_bound,
                                    &poll_timer)) != 0){
        print_error_message(exit_code);
      }
      else{
        offset_total += offset;
        error_bound_total += error_bound;
        s_counter++; // keep track of succesful requests
      }
    }
    // only show statistics if there has been more than zero succesful time
    // samples collected
    if (s_counter > 0){
      offset_avg = offset_total / s_counter;
      error_bound_avg = error_bound_total / s_counter;
      printf("\nStatistics -> offset average: %f, error bound average: +/- %f\n",
              offset_avg, error_bound_avg);
    }
    else{
      fprintf(stderr, "unable to collect any time samples\n");
    }
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
int unicast_mode(struct client_settings c_set, double *offset,
                double *error_bound, struct timeval *poll_timer){
  int sockfd; // client socket
  int debug = c_set.debug;
  int exit_code;
  int rem_time;
  int retry_count;
  int valid_reply;
  int no_recv_error;
  struct sockaddr_in reply_addr; // address of the server that has sent the packet
  struct host_info userver; // unicast server to request time from
  struct ntp_packet request_pkt; // request from client to server
  struct ntp_packet reply_pkt; // reply from server to client
  struct core_ts serv_ts; // core times

  // setup socket
  if ((exit_code = initialise_socket(&sockfd, c_set.recv_uni_timeout,
                                      c_set.debug)) != 0){
    return exit_code;
  }

  // connect to ntp server
  if ((exit_code = initialise_server_interface(c_set.server_host, c_set.server_port,
                                          &userver, c_set.debug)) != 0){
    return exit_code;
  }

  retry_count = 1;
  valid_reply = 0;
  // enforces the loop below to skip the first wait check, as no timer has been
  // set yet.
  poll_timer->tv_sec = -1;

  // keep retrying until a valid packet has been identified.
  while (!valid_reply){
    if (retry_count > c_set.max_unicast_retries){
      // stop trying and return an error
      return 4;
    }

    /* enforce a min wait between requests, to avoid getting blocked by ther server.
       note that the time between requests will be the value of recv_uni_timeout if it
       is larger number than poll_wait, as the timer starts before the
       request is sent.
    */
    if (poll_timer->tv_sec != -1 &&
            get_elapsed_time(*poll_timer) <= c_set.poll_wait){
      continue;
    }

    // build sntp request packet
    create_packet(&request_pkt);

    // start timer
    *poll_timer = start_timer();

    // send request packet to server
    if (send_SNTP_packet(&request_pkt, sockfd, userver.addr, debug) != 0){
      rem_time = c_set.poll_wait - get_elapsed_time(*poll_timer);
      print_debug(debug, "error sending request packet, can poll "
                  "again in %i second(s).",
                  (rem_time<0)?0:rem_time); // stops rem_time appearing below zero
      retry_count++;
      continue;
    }

    // listen for reply packets, keep listening even if a reply packet has
    // arrived from a different server than the request was sent to or
    // until an error occurs
    no_recv_error = 0;
    do {
      if (recieve_SNTP_packet(sockfd, &reply_pkt, &reply_addr,
                              &serv_ts.destination_timestamp, c_set.debug) != 0){
        rem_time = c_set.poll_wait - get_elapsed_time(*poll_timer);
        print_debug(debug, "error receiving reply packet, can poll "
                    "again in %i second(s).", (rem_time<0)?0:rem_time);
        no_recv_error = 1;
      }
    } while( !(no_recv_error) && is_same_ipaddr(userver.addr, reply_addr));

    // loop again if a recv error occured
    if (no_recv_error){
      retry_count++;
      continue;
    }

    // check reply packet is valid and trusted
    if (run_sanity_checks(request_pkt, reply_pkt, c_set) != 0){
      rem_time = c_set.poll_wait - get_elapsed_time(*poll_timer);
      print_debug(debug, "error running sanity checks, can poll "
                  "again in %i second(s).", (rem_time<0)?0:rem_time);
      retry_count++;
      continue;
    }
    valid_reply = 1;
  }

  get_timestamps_from_packet_in_epoch_time(&reply_pkt, &serv_ts);
  *offset = calculate_clock_offset(serv_ts);
  *error_bound = calculate_error_bound(serv_ts);
  print_server_results(serv_ts.transmit_timestamp, *offset, *error_bound,
                       userver, reply_pkt.stratum);

  close(sockfd);
  return 0;
}


double calculate_clock_offset(struct core_ts ts){
  double t1, t2, t3, t4;

  t1 = ts.originate_timestamp.tv_sec + (1.0e-6 * ts.originate_timestamp.tv_usec);
  t2 = ts.receive_timestamp.tv_sec + (1.0e-6 * ts.receive_timestamp.tv_usec);
  t3 = ts.transmit_timestamp.tv_sec + (1.0e-6 * ts.transmit_timestamp.tv_usec);
  t4 = ts.destination_timestamp.tv_sec + (1.0e-6 * ts.destination_timestamp.tv_usec);

  return ((t2 - t1) + (t3 - t4)) / 2;
}


double calculate_error_bound(struct core_ts ts){
  double t1, t2, t3, t4;

  t1 = ts.originate_timestamp.tv_sec + (1.0e-6 * ts.originate_timestamp.tv_usec);
  t2 = ts.receive_timestamp.tv_sec + (1.0e-6 * ts.receive_timestamp.tv_usec);
  t3 = ts.transmit_timestamp.tv_sec + (1.0e-6 * ts.transmit_timestamp.tv_usec);
  t4 = ts.destination_timestamp.tv_sec + (1.0e-6 * ts.destination_timestamp.tv_usec);

  return (t4 - t1) - (t3 - t2);
}


char * convert_epoch_time_to_human_readable(struct timeval epoch_time){
   char *readable_time_string;
   struct tm  ts;
   char time_convertion[35];

   readable_time_string = (char*)malloc(35);
   ts = *localtime(&epoch_time.tv_sec);
   strftime(time_convertion, sizeof(time_convertion), "%Y-%m-%d %H:%M:%S", &ts);
   sprintf(readable_time_string, "%s.%06li", time_convertion, epoch_time.tv_usec);
   return (char *)readable_time_string;
 }


void create_packet(struct ntp_packet *pkt){
  struct ntp_time_t transmit_ts_ntp;

  memset( pkt, 0, sizeof *pkt ); // zero all fields in struct

   // set SNTP V4 and Mode 3(client)
  pkt->li_vn_mode = (4 << 3) | 3; // (vn << 3) | mode

  transmit_ts_ntp = get_ntp_time_of_day();
  pkt->transmit_timestamp.second =  htonl(transmit_ts_ntp.second);
  pkt->transmit_timestamp.fraction = htonl(transmit_ts_ntp.fraction);
 }


int discover_unicast_servers_with_manycast(struct client_settings *c_set,
                                            char *ntp_servers[],
                                            int *s_count){
  int exit_code;
  int sockfd;
  struct sockaddr_in server; // discovered server
  struct host_info many_grp; // manycast group
  struct ntp_packet request_pkt; // request packet to manycast group
  struct ntp_packet reply_pkt; // reply packet from a manycast group server
  struct timeval timer; // use to track amount of time elapsed
  u_char ttl = 55; // time to live for manycast packets

  *s_count = 0;
  print_debug(c_set->debug, "initialising manycast request");

  // setup socket
  if ((exit_code = initialise_socket(&sockfd, MANYCAST_RECV_TIMEOUT,
                                       c_set->debug)) != 0){
    return exit_code;
  }

  if ((exit_code = initialise_server_interface(c_set->manycast_address,
                                               c_set->server_port,
                                               &many_grp, c_set->debug)) != 0){
    return exit_code;
  }

  if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0){
    print_debug(c_set->debug, "error setting ttl");
    return 7;
  }

  create_packet(&request_pkt);

  // send an ntp request to the manycast group
  if (send_SNTP_packet(&request_pkt, sockfd, many_grp.addr,
                       c_set->debug) != 0){
    print_debug(c_set->debug, "error sending manycast request packet");
    return 5;
  }

  timer = start_timer();
  // gather server replies for a set time
  while (get_elapsed_time(timer) <= c_set->manycast_wait_time){
    // listen for a server
    if (recieve_SNTP_packet(sockfd, &reply_pkt, &server, NULL,
                            c_set->debug) != 0){
      print_debug(c_set->debug, "no replys from any server");
      continue;
    }

    print_debug(c_set->debug, "server discovered: %s",
                inet_ntoa( server.sin_addr));

    // check the reply packet to test the state/health of the server
    if (run_sanity_checks(request_pkt, reply_pkt, *c_set) != 0){
      print_debug(c_set->debug, "server '%s' failed sanity checks, "
                 "discarding server", inet_ntoa( server.sin_addr));
      continue;
    }

    ntp_servers[*s_count] = inet_ntoa( server.sin_addr);
    ++*s_count; // inc number of servers found
    print_debug(c_set->debug, "server '%s' is approved",
                inet_ntoa( server.sin_addr));
  }

  // return an error if no servers are found
  if (*s_count == 0){
    print_debug(c_set->debug, "no servers found from manycast query");
    return 6;
  }

  close(sockfd);
  return 0;
 }


/*
  precedence order(from high to low):
    - commandline
    - config file
    - defaults
*/
struct client_settings get_client_settings(int argc, char * argv[]){
  struct client_settings c_set;

  // set relevant settings to their defaults
  c_set.server_port = DEFAULT_SERVER_PORT;
  c_set.debug = DEFAULT_DEBUG;
  c_set.recv_uni_timeout = DEFAULT_RECV_TIMEOUT;
  c_set.max_unicast_retries = DEFAULT_MAX_UNICAST_RETRY_LIMIT;
  c_set.poll_wait = DEFAULT_MIN_POLL_WAIT;
  c_set.timed_repeat_updates_enabled = DEFAULT_REPEAT_UPDATES_ENABLED;
  c_set.timed_repeat_updates_limit = DEFAULT_REPEAT_UPDATE_LIMIT;
  c_set.manycast_address = DEFAULT_MANYCAST_ADDRESS;
  c_set.manycast_wait_time = DEFAULT_MANYCAST_WAIT_TIME;

  // dont parse config file if it doesnt exist
  if (0 == access(CONFIG_FILE, 0)){
    // update settings from options defined in the config file
    parse_config_file(&c_set);
  }
  else{
    print_debug(c_set.debug, "no config file found for '%s'", CONFIG_FILE);
  }

  // update setting by arguments define din the commandline
  parse_command_line(argc, argv, &c_set);

  return c_set;
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


int initialise_socket(int *sockfd, int recv_uni_timeout, int debug){
  if( (*sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
    print_debug(debug, "error creating socket\n");
    return 3;
  }

  if (set_socket_recvfrom_timeout(*sockfd, recv_uni_timeout, debug) !=0){
    print_debug(debug, "error setting socket timeout");
    return 8;
  }
  return 0;
}


int initialise_server_interface(const char *host, int port, struct host_info *cn,
                            int debug){
  struct hostent *he;
  struct in_addr ipaddr;

  // check if address given is a ipaddress or hostname and check for existence
  if (inet_pton(AF_INET, host, &ipaddr) != 0){
    // is ipv4
    if( (he = gethostbyaddr(&ipaddr, sizeof(ipaddr),AF_INET)) == NULL){
      print_debug(debug, "unicast server not found");
      return 2;
    }

    cn->name = he->h_name;
  }
  else{
    // assume address is a hostname
    if( (he = gethostbyname( host)) == NULL) {
      print_debug(debug, "unicast server not found");
      return 2;
    }
    cn->name = host;
  }

  memset( &cn->addr,0, sizeof cn->addr); /* zero struct */
  cn->addr.sin_family = AF_INET;    /* host byte order .. */
  cn->addr.sin_port = htons( port); /* .. short, netwk byte order */
  cn->addr.sin_addr = *((struct in_addr *)he -> h_addr);
  return 0;
 }


int is_same_ipaddr(struct sockaddr_in sent_addr, struct sockaddr_in reply_addr){
  if (strcmp(inet_ntoa(reply_addr.sin_addr), inet_ntoa(sent_addr.sin_addr))){
    return 1;
  }
  else{
    return 0;
  }
}


void parse_command_line(int argc, char * argv[], struct client_settings *c_set){
  int many_set;
  int uni_set;
  int c;

  uni_set = 0;
  many_set = 0;
  while (optind < argc) {
    if ((c = getopt(argc, argv, "u:mp:dr:")) != -1) {
      switch(c) {
        case 'u':
          c_set->server_host = optarg;
          uni_set = 1;
          break;

        case 'm':
          many_set = 1;
          c_set->manycast_enabled = 1;
        //  c_set->manycast_address = optarg;
          break;

        case 'p':
          c_set->server_port = atoi(optarg);
          break;

        case 'd':
          c_set->debug = 1;
          break;

        case 'r':
          c_set->timed_repeat_updates_enabled = 1;
          c_set->timed_repeat_updates_limit = atoi(optarg);
          break;

        case '?':
          if (optopt == 'r'){
            fprintf(stderr ,"number of repeats not given for option '-r'\n");
            exit(1);
          }

        default:
          abort();
      }
    }
  }
  if (uni_set && many_set){
    fprintf(stderr , "%s: unicast address given as well as enabling "
          "manycast, only one or the other can be chosen\n", argv[0]);
    exit(1);
  }
}


void parse_config_file(struct client_settings *c_set){
  config_t cfg;

  cfg = setup_config_file(CONFIG_FILE); // get config file options

  // set the manycast address if manycast is enabled via the commandline
  config_lookup_string(&cfg, "manycast_address", &c_set->manycast_address);
  config_lookup_int(&cfg, "manycast_wait_time", &c_set->manycast_wait_time);

  config_lookup_int(&cfg, "server_port", &c_set->server_port);
  // set unicast socket timeout
  config_lookup_int(&cfg, "recv_uni_timeout", &c_set->recv_uni_timeout);
  // set max unicast retry limit
  config_lookup_int(&cfg, "max_unicast_retries", &c_set->max_unicast_retries);
  // set minimum time till polling the same server again
  config_lookup_int(&cfg, "poll_wait", &c_set->poll_wait);
}


void print_server_results(struct timeval transmit_time, double offset,
                          double error_bound, struct host_info cn,
                          int stratum){
  char *time_str;

  time_str = convert_epoch_time_to_human_readable(transmit_time);

  // output timestamp, clock offset and errorbound
  printf("%s (+0000) %f +/- %f ", time_str, offset, error_bound);
  // print hostname of the server if one exists
  if (cn.name){
    printf("%s ", cn.name);
  }
  // output ip address, stratum and no-leap
  printf("%s s%i no-leap\n", inet_ntoa( cn.addr.sin_addr), stratum);
}


void print_error_message(int error_code){
  char msg_start[50] = "error -";
  switch(error_code){
    case 2:
      fprintf( stderr,"%s server not found\n", msg_start);
      break;
    case 3:
      fprintf( stderr,"%s failed to create socket\n", msg_start);
      break;
    case 4:
      fprintf( stderr,"%s max number of retries hit\n", msg_start);
      break;
    case 5:
      fprintf( stderr, "%s sending manycast request packet\n", msg_start);
      break;
    case 6:
      fprintf( stderr, "%s no servers found from manycast query\n", msg_start);
      break;
    case 7:
      fprintf( stderr, "%s cant set ttl for manycast request\n", msg_start);
      break;
    case 8:
      fprintf( stderr, "%s cant set recv timeout for socket\n", msg_start);
      break;
    default:
      fprintf( stderr,"%s unknown(code=%i)\n", msg_start, error_code);
  }
}


int run_sanity_checks(struct ntp_packet req_pkt, struct ntp_packet rep_pkt,
                      struct client_settings c_set){
  int rep_mode;
  int rep_version;
  int req_version;
  char error_msg[100] = "sanity checks failed on -";

  rep_mode = rep_pkt.li_vn_mode & 0x7; // extract first 3 bits
  req_version = (req_pkt.li_vn_mode >> 3) & 0x7; // extract bits 3 to 5
  rep_version = (rep_pkt.li_vn_mode >> 3) & 0x7; // extract bits 3 to 5

  // the originate time in the server reply should be the same as the transmit
  // time in the request
  if ((req_pkt.transmit_timestamp.second != rep_pkt.originate_timestamp.second) ||
        (req_pkt.transmit_timestamp.fraction != rep_pkt.originate_timestamp.fraction)){
    print_debug(c_set.debug, "%s originate time in the server reply does not "
             "match the transmit time in the request.", error_msg);
    return 1;
  }

  // check stratum is in range
  else if (rep_pkt.stratum < 0 || rep_pkt.stratum > 15){
    print_debug(c_set.debug, "%s stratum is not in range 0 to 15(stratum=%i)",
                        error_msg, rep_pkt.stratum);
    return 1;
  }

  // transmit time in the reply packet cant be zero
  else if (rep_pkt.transmit_timestamp.second == 0 &&
                rep_pkt.transmit_timestamp.fraction == 0){
    print_debug(c_set.debug, "%s transmit time of reply packet is zero",
                        error_msg);
    return 1;
  }

  // check mode is 4(server)
  else if (rep_mode != 4){
    print_debug(c_set.debug, "%s mode of reply packet is not server(mode=%i)",
                        error_msg, rep_mode);
    return 1;
  }

  // server must be the same version as the client. This check irradicates
  // the need to check if the version is non-zero as the client can never
  // be non-zero.
  else if (req_version != rep_version){
    print_debug(c_set.debug, "%s server should be of the same version "
                        "as the client", error_msg);
    return 1;
  }

  return 0;


}


struct timeval start_timer(){
  struct timeval start_time;

  gettimeofday(&start_time, NULL);
  return start_time;
}

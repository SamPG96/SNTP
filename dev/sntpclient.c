/* talker.c - a datagram 'client'
 * need to supply host name/IP and one word message,
 * e.g. talker localhost hello
 *
 */
#include "timeconvertion.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>         /* for gethostbyname() */

#define PORT 123     /* server port the client connects to */
#define MAXBUFLEN 200

struct ntp_packet {
  uint8_t li_vn_mode;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;
  uint32_t root_delay;
  uint32_t root_dispersion;
  uint32_t reference_identifier;
  struct ntp_time_t reference_timestamp;
  struct ntp_time_t originate_timestamp;
  struct ntp_time_t receive_timestamp;
  struct ntp_time_t transmit_timestamp;
};

// stores commonly used timestamps in epoch time
struct core_ts {
  struct timeval originate_timestamp;
  struct timeval receive_timestamp;
  struct timeval transmit_timestamp;
  struct timeval destination_timestamp;
};

struct connection_info{
  char *name; //hostname
  struct sockaddr_in addr;
  int sockfd;
};

double calculate_clock_offset(struct core_ts ts);
void close_connection(struct connection_info cn);
char * convert_epoch_time_to_human_readable(struct timeval epoch_time);
void create_packet(struct ntp_packet *pkt);
void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt, struct core_ts *ts );
int initialise_connection_to_server(char addr[], struct connection_info *cn );
void print_server_results(struct core_ts ts, struct connection_info cn, int stratum);
int process_cmdline(int argc, char * argv[]);
int recieve_SNTP_packet(struct ntp_packet *pkt, struct connection_info cn,
                        struct core_ts *ts);
int send_SNTP_packet(struct ntp_packet *pkt, struct connection_info cn);




int main( int argc, char * argv[]) {
  struct connection_info server_connection;
  struct ntp_packet request_pkt; // request from client to server
  struct ntp_packet response_pkt; // response from server to client
  struct core_ts serv_ts; // core times

  // check cmd line arguments
  if (process_cmdline(argc, argv) != 0){
    exit(1);
  }

  // connect to ntp server
  if (initialise_connection_to_server(argv[ 1], &server_connection) != 0){
    exit(1);
  }

  // build sntp request packet
  create_packet(&request_pkt);

  // send request packet to server
  if (send_SNTP_packet(&request_pkt, server_connection) != 0){
    exit(1);
  }

  // recieve NTP response packet from server
  if (recieve_SNTP_packet(&response_pkt, server_connection, &serv_ts) != 0){
    exit(1);
  }

  get_timestamps_from_packet_in_epoch_time(&response_pkt, &serv_ts);

  // TODO: run checks on packet here

  print_server_results(serv_ts, server_connection, response_pkt.stratum);

  close_connection(server_connection);
  return 0;
}


double calculate_clock_offset(struct core_ts ts){
  double t1 = ts.originate_timestamp.tv_sec + ((double) ts.originate_timestamp.tv_usec / 1000000);
  double t2 = ts.receive_timestamp.tv_sec + ((double) ts.receive_timestamp.tv_usec / 1000000);
  double t3 = ts.transmit_timestamp.tv_sec + ((double) ts.transmit_timestamp.tv_usec / 1000000);
  double t4 = ts.destination_timestamp.tv_sec + ((double) ts.destination_timestamp.tv_usec / 1000000);

  return ((t2 - t1) + (t3 - t4)) / 2;
}


void close_connection(struct connection_info cn){
  close( cn.sockfd);
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
  struct timeval epoch;
  struct ntp_time_t ntp;

  memset( pkt, 0, sizeof *pkt );
   // set SNTP V4 and Mode 3(client)
  pkt->li_vn_mode = (4 << 3) | 3; // (vn << 3) | mode
  gettimeofday(&epoch, NULL);
  convert_unix_time_into_ntp_time(&epoch, &ntp);
  pkt->transmit_timestamp.second =  htonl(ntp.second);
  pkt->transmit_timestamp.fraction = htonl(ntp.fraction);
 }


void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt, struct core_ts *ts ){
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


int initialise_connection_to_server(char addr[], struct connection_info *cn){
  struct hostent *he;
  struct sockaddr_in their_addr;    /* server address info */

  /* resolve server host name or IP address */
  if( (he = gethostbyname( addr)) == NULL) {
    perror( "initialise_connection_to_server: host not found");
    return 1;
  }

  if( (cn->sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror( "initialise_connection_to_server: error creating socket");
    return 1;
  }

  memset( &their_addr,0, sizeof their_addr); /* zero struct */
  their_addr.sin_family = AF_INET;    /* host byte order .. */
  their_addr.sin_port = htons( PORT); /* .. short, netwk byte order */
  their_addr.sin_addr = *((struct in_addr *)he -> h_addr);

  // get server hostname, if one exists
  cn->name = gethostbyaddr((char *)&their_addr.sin_addr, sizeof(
                          their_addr.sin_addr), their_addr.sin_family)->h_name;
  cn->addr = their_addr;
  return 0;
 }


void print_server_results(struct core_ts ts, struct connection_info cn, int stratum){
  char *time_str;
  double offset;

  time_str = convert_epoch_time_to_human_readable(ts.transmit_timestamp);
  offset = calculate_clock_offset(ts);
  // TODO: check what (+0000) is
  // TODO: add errorbound

  // output timestamp, clock offset and errorbound
  printf("%s (+0000) %f +/- ERRORBOUND(TODO) ", time_str, offset);
  // print hostname of the server if one exists
  if (cn.name){
    printf("%s ", cn.name);
  }
  // output ip address, stratum and no-leap
  printf("%s s%i no-leap\n", inet_ntoa( cn.addr.sin_addr), stratum);
}


int process_cmdline(int argc, char * argv[]){
    if( argc != 2) {
      fprintf( stderr, "usage: %s hostname\n", argv[0]);
      return 1;
    }
    return 0;
  }


int recieve_SNTP_packet(struct ntp_packet *pkt, struct connection_info cn,
                        struct core_ts *ts){
  int addr_len;
  int numbytes;

  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( cn.sockfd, pkt, MAXBUFLEN - 1, 0,
               (struct sockaddr *)&cn.addr, &addr_len)) == -1) {
    perror( "recieve_SNTP_packet: recvfrom");
    return  1;
  }
  gettimeofday(&ts->destination_timestamp, NULL);
  printf( "Got packet from %s\n", inet_ntoa( cn.addr.sin_addr));
  printf( "Recieved packet is %d bytes long\n\n", numbytes);
  return 0;
}


int send_SNTP_packet(struct ntp_packet *pkt, struct connection_info cn){
  int numbytes;
  if( (numbytes = sendto( cn.sockfd, pkt, 48, 0, //48 TODO: make sizeof pkt work
      (struct sockaddr *)&cn.addr, sizeof( struct sockaddr))) == -1) {
    perror( "send_SNTP_packet: sendto");
    return  1;
  }
  printf( "Sent %d bytes to %s\n", numbytes,
                          inet_ntoa( cn.addr.sin_addr));
  return 0;
}

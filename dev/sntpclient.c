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
 // unsigned version;
  uint8_t li_vn_mode;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;
  uint32_t root_delay;
  uint32_t root_dispersion;
  uint32_t reference_identifier;
  uint32_t reference_timestamp_secs;
  uint32_t reference_timestamp_fraq;
  uint32_t originate_timestamp_secs;
  uint32_t originate_timestamp_fraq;
  uint32_t receive_timestamp_secs;
  uint32_t receive_timestamp_fraq;
  uint32_t transmit_timestamp_secs;
  uint32_t transmit_timestamp_fraq;
};

// stores key timestamps in epoch time
struct timestamps {
  struct timeval originate_timestamp;
  struct timeval receive_timestamp;
  struct timeval transmit_timestamp;
  struct timeval destination_timestamp;
};

double calculate_clock_offset(struct timestamps ts);
void create_packet(struct ntp_packet *pkt);
void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt, struct timestamps *ts );
int initialise_connection_to_server(char hostanme[],
                                    struct sockaddr_in *their_addr, int *sockfd);
char * print_epoch_time_in_human_readable_form(struct timeval epoch_time);
void print_packet(struct ntp_packet *pkt);
void print_server_results(struct timestamps ts, char hostname[]);
int process_cmdline(int argc, char * argv[]);
int recieve_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                        int *sockfd, struct timestamps *ts);
int send_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                     int *sockfd);

int main( int argc, char * argv[]) {
  int sockfd;
  struct ntp_packet request_pkt; // request from client to server
  struct ntp_packet response_pkt; // response from server to client
  struct sockaddr_in their_addr;    /* server address info */
  struct timestamps serv_timestamps;

  // check cmd line arguments
  if (process_cmdline(argc, argv) != 0){
    exit(1);
  }

  // connect to ntp server
  if (initialise_connection_to_server(argv[ 1], &their_addr, &sockfd) != 0){
    exit(1);
  }

  // build sntp request packet
  create_packet(&request_pkt);

  // send request packet to server
  if (send_SNTP_packet(&request_pkt, &their_addr, &sockfd) != 0){
    exit(1);
  }

  // recieve NTP response packet from server
  if (recieve_SNTP_packet(&response_pkt, &their_addr, &sockfd, &serv_timestamps) != 0){
    exit(1);
  }

  get_timestamps_from_packet_in_epoch_time(&response_pkt, &serv_timestamps);

  // TODO: run checks on packet here

  print_server_results(serv_timestamps, argv[ 1]);

  close( sockfd);
  return 0;
}


double calculate_clock_offset(struct timestamps ts){
  double t1 = ts.originate_timestamp.tv_sec + ((double) ts.originate_timestamp.tv_usec / 1000000);
  double t2 = ts.receive_timestamp.tv_sec + ((double) ts.receive_timestamp.tv_usec / 1000000);
  double t3 = ts.transmit_timestamp.tv_sec + ((double) ts.transmit_timestamp.tv_usec / 1000000);
  double t4 = ts.destination_timestamp.tv_sec + ((double) ts.destination_timestamp.tv_usec / 1000000);

  return ((t2 - t1) + (t3 - t4)) / 2;
}


void create_packet(struct ntp_packet *pkt){
  struct timeval epoch;
  struct ntp_time_t ntp;

  memset( pkt, 0, sizeof *pkt );
   // set SNTP V4 and Mode 3(client)
  pkt->li_vn_mode = (4 << 3) | 3; // (vn << 3) | mode
  gettimeofday(&epoch, NULL);
  convert_unix_time_into_ntp_time(&epoch, &ntp);
  pkt->transmit_timestamp_secs = htonl(ntp.second) ;
  pkt->transmit_timestamp_fraq = htonl(ntp.fraction);
 }


void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt, struct timestamps *ts ){
  struct ntp_time_t originate_timestamp_ntp;
  struct ntp_time_t receive_timestamp_ntp;
  struct ntp_time_t transmit_timestamp_ntp;

  originate_timestamp_ntp.second = ntohl(pkt->originate_timestamp_secs);
  originate_timestamp_ntp.fraction = ntohl(pkt->originate_timestamp_fraq);
  convert_ntp_time_into_unix_time(&originate_timestamp_ntp, &ts->originate_timestamp);

  receive_timestamp_ntp.second = ntohl(pkt->receive_timestamp_secs);
  receive_timestamp_ntp.fraction = ntohl(pkt->receive_timestamp_fraq);
  convert_ntp_time_into_unix_time(&receive_timestamp_ntp, &ts->receive_timestamp);

  transmit_timestamp_ntp.second = ntohl(pkt->transmit_timestamp_secs);
  transmit_timestamp_ntp.fraction = ntohl(pkt->transmit_timestamp_fraq);
  convert_ntp_time_into_unix_time(&transmit_timestamp_ntp, &ts->transmit_timestamp);
}


int initialise_connection_to_server(char hostname[],
                                     struct sockaddr_in *their_addr,
                                     int *sockfd){
   struct hostent *he;

   /* resolve server host name or IP address */
   if( (he = gethostbyname( hostname)) == NULL) {
     perror( "simpleclient gethostbyname");
     return 1;
   }

   if( (*sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
     perror( "simpleclient socket");
     return 1;
   }

   memset( their_addr,0, sizeof *their_addr); /* zero struct */
   their_addr->sin_family = AF_INET;    /* host byte order .. */
   their_addr->sin_port = htons( PORT); /* .. short, netwk byte order */
   their_addr->sin_addr = *((struct in_addr *)he -> h_addr);

   return 0;
 }


char * print_epoch_time_in_human_readable_form(struct timeval epoch_time){
   char *readable_time_string;
   struct tm  ts;
   char time_convertion[35];

   readable_time_string = (char*)malloc(35);
   ts = *localtime(&epoch_time.tv_sec);
   strftime(time_convertion, sizeof(time_convertion), "%Y-%m-%d %H:%M:%S", &ts);
   sprintf(readable_time_string, "%s.%li", time_convertion, epoch_time.tv_usec);
   return (char *)readable_time_string;
 }


void print_server_results(struct timestamps ts, char hostname[]){
  char *time_str;
  double offset;

  time_str = print_epoch_time_in_human_readable_form(ts.transmit_timestamp);
  offset = calculate_clock_offset(ts);
  // TODO: check what (+0000) is
  // TODO: add errorbound
  // TODO: show hostanme and/or IP of server
  // TODO: add stratum
  // TODO: add no-leap
  printf("%s (+0000) %f +/- ERRORBOUND(TODO) %s\n", time_str, offset, hostname);
}


int process_cmdline(int argc, char * argv[]){
    if( argc != 2) {
      fprintf( stderr, "usage: simpleclient hostname\n");
      return 1;
    }
    return 0;
  }


int recieve_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                        int *sockfd, struct timestamps *ts){
  int addr_len;
  int numbytes;
  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( *sockfd, pkt, MAXBUFLEN - 1, 0,
               (struct sockaddr *)their_addr, &addr_len)) == -1) {
    perror( "Listener recvfrom");
    return  1;
  }
  gettimeofday(&ts->destination_timestamp, NULL);
  printf( "Got packet from %s\n", inet_ntoa( their_addr->sin_addr));
  printf( "Recieved packet is %d bytes long\n\n", numbytes);
  return 0;
}


int send_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                          int *sockfd){
  int numbytes;
  if( (numbytes = sendto( *sockfd, pkt, 48, 0, //48 TODO: make sizeof pkt work
      (struct sockaddr *)their_addr, sizeof( struct sockaddr))) == -1) {
    perror( "Talker sendto");
    return  1;
  }
  printf( "Sent %d bytes to %s\n", numbytes,
                          inet_ntoa( their_addr->sin_addr));
  return 0;
}

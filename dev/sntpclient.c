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

#define NTP_EPOCH       (86400U * (365U * 70U + 17U))

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

double calculate_clock_offset(struct ntp_packet server_pkt, struct timeval destination_time);
void create_packet(struct ntp_packet *pkt);
int initialise_connection_to_server(char hostanme[],
                                    struct sockaddr_in *their_addr, int *sockfd);
void print_epoch_time_in_human_readable_form(struct timeval *epoch_time);
void print_server_results(struct ntp_packet *pkt);
int process_cmdline(int argc, char * argv[]);
int recieve_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                        int *sockfd, struct timeval *server_reply_epoch_time);
int send_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                     int *sockfd);

int main( int argc, char * argv[]) {
  int sockfd;
  struct ntp_packet request_pkt; // request from client to server
  struct ntp_packet response_pkt; // response from server to client
  struct sockaddr_in their_addr;    /* server address info */
  struct timeval server_reply_epoch_time;

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
  if (recieve_SNTP_packet(&response_pkt, &their_addr, &sockfd, &server_reply_epoch_time) != 0){
    exit(1);
  }

  // TODO: run checks on packet here
  //printf("Offset: %f\n", calculate_clock_offset(response_pkt, server_reply_epoch_time));
  print_server_results(&response_pkt);

  close( sockfd);
  return 0;
}


/*double calculate_clock_offset(struct ntp_packet server_pkt, struct timeval destination_time){
  // convert bits?
  double t1 = ntohl(server_pkt.originate_timestamp_secs) + ntohl(server_pkt.originate_timestamp_fraq);
  double t2 = ntohl(server_pkt.receive_timestamp_secs) + ntohl(server_pkt.receive_timestamp_fraq);
  double t3 = ntohl(server_pkt.transmit_timestamp_secs) + ntohl(server_pkt.transmit_timestamp_fraq);
  double t4 = destination_time.tv_sec + destination_time.tv_usec;
  return ((t2 - t1) + (t3 - t4)) / 2;
}*/


void create_packet(struct ntp_packet *pkt){
  struct timeval epoch;
  struct ntp_time_t ntp;

  memset( pkt, 0, sizeof *pkt );
   // set SNTP V4 and Mode 3(client)
  pkt->li_vn_mode = (4 << 3) | 3; // (vn << 3) | mode
  gettimeofday(&epoch, NULL);
  convert_nix_time_into_ntp_time(&epoch, &ntp);
  pkt->transmit_timestamp_secs = htonl(ntp.second) ;
  pkt->transmit_timestamp_fraq = htonl(ntp.fraction);
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


void print_epoch_time_in_human_readable_form(struct timeval *epoch_time){
   char readable_time_string[30];
   struct tm  ts;

   ts = *localtime(&epoch_time->tv_sec);
   strftime(readable_time_string, sizeof(readable_time_string), "%Y-%m-%d %H:%M:%S", &ts);
   printf("%s.%ld\n", readable_time_string, epoch_time->tv_usec);
 }


void print_server_results(struct ntp_packet *pkt){
  struct timeval transmit_time_epoch;
  struct ntp_time_t  transmit_time_NTP;

  transmit_time_NTP.second = ntohl(pkt->transmit_timestamp_secs);
  transmit_time_NTP.fraction = ntohl(pkt->transmit_timestamp_fraq);
  convert_ntp_time_into_unix_time(&transmit_time_NTP, &transmit_time_epoch);
  print_epoch_time_in_human_readable_form(&transmit_time_epoch);
  //  printf("NTP time: %u %u\n", ref_time.second, ref_time.fraction);
  //  printf("UNIX Time: %ld %ld\n", epoch.tv_sec, epoch.tv_usec);
}


int process_cmdline(int argc, char * argv[]){
    if( argc != 2) {
      fprintf( stderr, "usage: simpleclient hostname\n");
      return 1;
    }
    return 0;
  }


int recieve_SNTP_packet(struct ntp_packet *pkt, struct sockaddr_in *their_addr,
                        int *sockfd, struct timeval *server_reply_epoch_time){
  int addr_len;
  int numbytes;
  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( *sockfd, pkt, MAXBUFLEN - 1, 0,
               (struct sockaddr *)their_addr, &addr_len)) == -1) {
    perror( "Listener recvfrom");
    return  1;
  }
  gettimeofday(server_reply_epoch_time, NULL);
  printf( "Got packet from %s\n", inet_ntoa( their_addr->sin_addr));
  printf( "Recieved packet is %d bytes long\n", numbytes);
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

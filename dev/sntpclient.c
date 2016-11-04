/* talker.c - a datagram 'client'
 * need to supply host name/IP and one word message,
 * e.g. talker localhost hello
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>         /* for gethostbyname() */

#define PORT 123     /* server port the client connects to */
#define MAXBUFLEN 200

//#define NTP_EPOCH       (86400U * (365U * 70U + 17U))

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

struct ntp_time_t {
    uint32_t   second;
    uint32_t    fraction;
};

/*struct timeval {
    uint32_t   tv_sec;
    uint32_t    tv_usec;
};*/

void convert_ntp_time_into_unix_time(struct ntp_time_t *ntp, struct timeval *nix)
{
    nix->tv_sec = ntp->second - 0x83AA7E80; // the seconds from Jan 1, 1900 to Jan 1, 1970
    nix->tv_usec = (uint32_t)( (double)ntp->fraction * 1.0e6 / (double)(1LL<<32) );
}

int main( int argc, char * argv[]) {
  int sockfd, numbytes;
  int addr_len;
  struct ntp_packet pkt;
  struct hostent *he;
  struct sockaddr_in their_addr;    /* server address info */

  struct ntp_time_t ref_time;
  struct timeval nix;

  memset( &pkt, 0, sizeof pkt  );
  // set SNTP V4 and Mode 3(client)
  pkt.li_vn_mode = (4 << 3) | 3; // (vn << 3) | mode
  printf( "Mode of packet to send: %u\n", pkt.li_vn_mode);
//  pkt.transmit_timestamp_secs = time(0) + NTP_EPOCH;

  if( argc != 2) {
    fprintf( stderr, "usage: simpleclient hostname\n");
    exit( 1);
  }
  /* resolve server host name or IP address */
  if( (he = gethostbyname( argv[ 1])) == NULL) {
    perror( "simpleclient gethostbyname");
    exit( 1);
  }

  if( (sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror( "simpleclient socket");
    exit( 1);
  }

  memset( &their_addr,0, sizeof(their_addr)); /* zero struct */
  their_addr.sin_family = AF_INET;    /* host byte order .. */
  their_addr.sin_port = htons( PORT); /* .. short, netwk byte order */
  their_addr.sin_addr = *((struct in_addr *)he -> h_addr);

  // send NTP packet
  if( (numbytes = sendto( sockfd, &pkt, 48, 0, //48 TODO: make sizeof pkt work
      (struct sockaddr *)&their_addr, sizeof( struct sockaddr))) == -1) {
      perror( "Talker sendto");
      exit( 1);
  }

  printf( "Sent %d bytes to %s\n", numbytes,
                          inet_ntoa( their_addr.sin_addr));

  // get response packet
  addr_len = sizeof( struct sockaddr);
  if( (numbytes = recvfrom( sockfd, &pkt, MAXBUFLEN - 1, 0,
                (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror( "Listener recvfrom");
      exit( 1);
  }

  printf( "Got packet from %s\n", inet_ntoa( their_addr.sin_addr));
  printf( "Recieved packet is %d bytes long\n", numbytes);

  ref_time.second = ntohl(pkt.reference_timestamp_secs);
  ref_time.fraction = ntohl(pkt.reference_timestamp_fraq);
  convert_ntp_time_into_unix_time(&ref_time, &nix);
  printf("NTP time: %ld %ld\n", ref_time.second, ref_time.fraction);
  printf("UNIX Time: %ld %ld\n", nix.tv_sec, nix.tv_usec);
//  printf( "Ref timestamp is: %u\n", pkt.secs

  close( sockfd);
  return 0;
}

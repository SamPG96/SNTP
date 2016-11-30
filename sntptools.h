#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "reusedlib.h" // reused code found online


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

struct connection_info{
  char *name; //hostname
  struct sockaddr_in addr;
  int sockfd;
};

void close_connection(struct connection_info cn);
int send_SNTP_packet(struct ntp_packet *pkt, int sockfd, struct sockaddr_in addr);

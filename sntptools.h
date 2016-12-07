#include "reusedlib.h" // reused code found online
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


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

struct host_info{
  const char *name; //hostname
  struct sockaddr_in addr;
};

struct ntp_time_t get_ntp_time_of_day();
int send_SNTP_packet(struct ntp_packet *pkt, int sockfd, struct sockaddr_in addr,
                     int debug_enabled);

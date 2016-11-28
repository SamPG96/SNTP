#include <stdint.h>
#include <sys/socket.h>

#ifndef   REUSEDLIB_H
#define   REUSEDLIB_H
#endif

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

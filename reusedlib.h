#include <libconfig.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct ntp_time_t {
    uint32_t   second;
    uint32_t   fraction;
};

// http://waitingkuo.blogspot.co.uk/2012/06/conversion-between-ntp-time-and-unix.html
void convert_ntp_time_into_unix_time(struct ntp_time_t *ntp, struct timeval *epoch);
void convert_unix_time_into_ntp_time(struct timeval *epoch, struct ntp_time_t *ntp);

// http://stackoverflow.com/questions/2876024/linux-is-there-a-read-or-recv-from-socket-with-timeout
void set_socket_recvfrom_timeout(int sockfd, int seconds);

// https://github.com/hyperrealm/libconfig/blob/master/examples/c/example1.c
config_t setup_config_file(char *config_file);

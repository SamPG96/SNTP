#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>

struct ntp_time_t {
    uint32_t   second;
    uint32_t   fraction;
};

void convert_ntp_time_into_unix_time(struct ntp_time_t *ntp, struct timeval *epoch);
void convert_unix_time_into_ntp_time(struct timeval *epoch, struct ntp_time_t *ntp);


// http://waitingkuo.blogspot.co.uk/2012/06/conversion-between-ntp-time-and-unix.html

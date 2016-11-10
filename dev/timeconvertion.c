/*#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>

struct ntp_time_t {
    uint32_t   second;
    uint32_t    fraction;
};*/

#include "timeconvertion.h"

void convert_ntp_time_into_unix_time(struct ntp_time_t *ntp, struct timeval *epoch)
{
    epoch->tv_sec = ntp->second - 0x83AA7E80; // the seconds from Jan 1, 1900 to Jan 1, 1970
    epoch->tv_usec = (uint32_t)( (double)ntp->fraction * 1.0e6 / (double)(1LL<<32) );
}

void convert_nix_time_into_ntp_time(struct timeval *epoch, struct ntp_time_t *ntp)
{
    ntp->second = epoch->tv_sec + 0x83AA7E80;
    ntp->fraction = (uint32_t)( (double)(epoch->tv_usec+1) * (double)(1LL<<32) * 1.0e-6 );
}

/*int main()
{
    struct ntp_time_t ntp;
    struct timeval nix;

    // get time nix time via gettimeofday
    gettimeofday(&nix, NULL);
    printf("UNIX Time: %ld %ld\n", nix.tv_sec, nix.tv_usec);

    // convert nix time to ntp time
    convert_nix_time_into_ntp_time(&nix, &ntp);
    printf("NTP Time: %ld %ld\n", ntp.second, ntp.fraction);

    // convert ntp time back to nix time to see whether they are same
    convert_ntp_time_into_nix_time(&ntp, &nix);
    printf("UNIX Time: %ld %ld\n", nix.tv_sec, nix.tv_usec);
}*/

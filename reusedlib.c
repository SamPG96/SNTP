#include "reusedlib.h"


void convert_ntp_time_into_unix_time(struct ntp_time_t *ntp, struct timeval *epoch)
{
    epoch->tv_sec = ntp->second - 0x83AA7E80; // the seconds from Jan 1, 1900 to Jan 1, 1970
    epoch->tv_usec = (uint32_t)( (double)ntp->fraction * 1.0e6 / (double)(1LL<<32) );
}

void convert_unix_time_into_ntp_time(struct timeval *epoch, struct ntp_time_t *ntp)
{
    ntp->second = epoch->tv_sec + 0x83AA7E80;
    ntp->fraction = (uint32_t)( (double)(epoch->tv_usec+1) * (double)(1LL<<32) * 1.0e-6 );
}

int set_socket_recvfrom_timeout(int sockfd, int seconds, int debug){
  struct timeval tv;

  tv.tv_sec = seconds;  /* 30 Secs Timeout */
  tv.tv_usec = 0;  // Not init'ing this can cause strange errors

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
                 sizeof(struct timeval)) != 0){
    return 1;
  }
  return 0;
}

config_t setup_config_file(char *config_file){
  config_t cfg;

  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */
  if(! config_read_file(&cfg, config_file))
  {
   fprintf(stderr, "error reading config file(%s:%d - %s)\n",
           config_error_file(&cfg), config_error_line(&cfg),
           config_error_text(&cfg));
   config_destroy(&cfg);
   exit(1); // exit out if there is an issue with the config file
  }
  return cfg;
}


void print_debug(int enable_debug, const char *fmt, ...){
  char message[4096];
  va_list args;

  if (enable_debug != 1){
    return;
  }

  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  printf("DEBUG: %s\n", message);
}

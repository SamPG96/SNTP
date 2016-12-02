#include "sntptools.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>         /* for gethostbyname() */

// stores commonly used timestamps in epoch time
struct core_ts {
  struct timeval originate_timestamp;
  struct timeval receive_timestamp;
  struct timeval transmit_timestamp;
  struct timeval destination_timestamp;
};

// stores all crucial settings for the client
struct client_settings{
  char *server_host;
  int server_port;
  int debug_enabled;
  int recv_timeout;   //seconds
  int max_unicast_retries;
  int poll_wait;  // seconds
  int timed_repeat_updates_enabled;
  int timed_repeat_updates_limit;
};


#define CONFIG_FILE "config.cfg"

/*
  These defaults can be overwritten by the config file and command line
*/

// max number of retries for a single unicast request
#define DEFAULT_MAX_UNICAST_RETRY_LIMIT 2
// min number of seconds between polling the same server
#define DEFAULT_MIN_POLL_WAIT 15
// server port the client connects to
#define DEFAULT_SERVER_PORT 123
// produce more detailed output
#define DEFAULT_DEBUG_ENABLED 0
// seconds to wait for a server response
#define DEFAULT_RECV_TIMEOUT 10
// whether to enable pr to disable repeated updates
#define DEFAULT_REPEAT_UPDATES_ENABLED 0
// the maximum number of occasions to fetch updates of the server time
#define DEFAULT_REPEAT_UPDATE_LIMIT 4

#define MAXBUFLEN 200



double calculate_clock_offset(struct core_ts ts);
double calculate_error_bound(struct core_ts ts);
char * convert_epoch_time_to_human_readable(struct timeval epoch_time);
void create_packet(struct ntp_packet *pkt);
struct client_settings get_client_settings(int argc, char * argv[]);
int get_elapsed_time(struct timeval start_time);
void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt, struct core_ts *ts );
int initialise_udp_transfer(struct client_settings c_set, struct host_info *cn );
void parse_config_file(struct client_settings *c_set);
void print_debug(int enable_debug, const char *fmt, ...);
void print_server_results(struct timeval transmit_time, double offset,
                          double error_bound, struct host_info cn,
                          int stratum);
void print_unicast_error(int error_code);
int process_cmdline(int argc, char * argv[]);
int recieve_SNTP_packet(struct ntp_packet *pkt, struct host_info cn,
                        struct core_ts *ts, struct client_settings c_set);
int run_sanity_checks(struct ntp_packet req_pkt, struct ntp_packet rep_pkt,
                      struct client_settings c_set);
struct timeval start_timer();
int unicast_mode(struct client_settings c_set, double *offset, double *error_bound);

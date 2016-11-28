#include "reusedlib.h"
#include "sntpcommon.h"
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

struct connection_info{
  char *name; //hostname
  struct sockaddr_in addr;
  int sockfd;
};

// stores all crucial settings for the client
struct client_settings{
  char *server_host;
  int server_port;
  int recv_timeout;   //seconds
  int max_unicast_retries;
  int poll_wait;  // seconds
  int repeat_update_limit;
};


#define CONFIG_FILE "config.cfg"

// max number of retries for trying to get a reply from a unicast server
#define DEFAULT_MAX_UNICAST_RETRY_LIMIT 2
// min number of seconds between polling the same server
#define DEFAULT_MIN_POLL_WAIT 15
// server port the client connects to
#define DEFAULT_SERVER_PORT 123
// seconds to wait for a server response
#define DEFAULT_RECV_TIMEOUT 10
// the maximum number of occasions to fetch updates of the server time
#define DEFAULT_REPEAT_UPDATE_LIMIT 3

#define MAXBUFLEN 200



double calculate_clock_offset(struct core_ts ts);
void close_connection(struct connection_info cn);
char * convert_epoch_time_to_human_readable(struct timeval epoch_time);
void create_packet(struct ntp_packet *pkt);
struct client_settings get_client_settings(int argc, char * argv[]);
int get_elapsed_time(struct timeval start_time);
void get_timestamps_from_packet_in_epoch_time(struct ntp_packet *pkt, struct core_ts *ts );
int initialise_connection_to_server(struct client_settings c_settings, struct connection_info *cn );
void print_server_results(struct core_ts ts, struct connection_info cn, int stratum);
int process_cmdline(int argc, char * argv[]);
int recieve_SNTP_packet(struct ntp_packet *pkt, struct connection_info cn,
                        struct core_ts *ts);
int run_sanity_checks(struct ntp_packet req_pkt, struct ntp_packet rep_pkt);
int send_SNTP_packet(struct ntp_packet *pkt, struct connection_info cn);
struct timeval start_timer();
int unicast_request(struct client_settings c_settings);

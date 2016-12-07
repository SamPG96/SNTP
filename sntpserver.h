#include "sntptools.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct sntp_request{
  struct host_info client;
  struct ntp_packet pkt;
  struct ntp_time_t time_of_request;
};


// stores all crucial settings for the server
struct server_settings{
  int server_port;
  int debug_enabled;
  int manycast_enabled;
  const char *manycast_address;
};


struct ntp_packet create_reply_packet(struct sntp_request *c_req);
struct server_settings get_server_settings(int argc, char * argv[]);
int initialise_server(int *sockfd, int port, struct host_info *cn);
void parse_config_file(struct server_settings *s_set);
int setup_manycast(int sockfd, const char *manycast_address);


#define CONFIG_FILE "server_config.cfg"

// set default settings
#define DEFAULT_DEBUG_ENABLED 0
#define DEFAULT_MANYCAST_ENABLED 0
#define DEFAULT_MANYCAST_ADDRESS "224.0.1.1"
#define DEFAULT_SERVER_PORT 6001

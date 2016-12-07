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

struct ntp_packet create_reply_packet(struct sntp_request *c_req);
int initialise_server(int *sockfd, struct host_info *cn);
int setup_multicast(int sockfd, struct host_info cn);

#define MYPORT 6001        /* the port users connect to */
#define MULTICAST_ADDRESS "224.0.1.1"

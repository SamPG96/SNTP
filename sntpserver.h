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

#define MYPORT 6001        /* the port users connect to */
#define MULTICAST_ADDRESS "224.0.1.1"
#define MAXBUFLEN 100

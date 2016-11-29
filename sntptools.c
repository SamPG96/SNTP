#include "sntptools.h"

void close_connection(struct connection_info cn){
  close( cn.sockfd);
}

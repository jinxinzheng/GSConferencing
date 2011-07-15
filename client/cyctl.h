#ifndef __CYCTL_H__
#define __CYCTL_H__

#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "ax88783ctl.h"

static inline void do_cyctl(int open)
{
  int fd;
  struct ifreq rq;

  if( (fd = socket(AF_INET,SOCK_DGRAM,0)) >= 0 )
  {
    strcpy(rq.ifr_name, "eth0");
    rq.ifr_data = (void *) ((1<<8) | open);
    if( (ioctl(fd, AX88783_IOCS_OPEN_PORT, (char *) &rq)) )
    {
      perror("ioctl");
    }
    close(fd);
  }
}

#endif  /*__CYCTL_H__*/

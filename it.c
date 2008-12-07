/*
 * itunnel - an ICMP tunnel by edi / teso
 * usage: it [-i id] [-s packetsize] host
 * establishes   a   bidirectional   ICMP 
 * 'connection' with 'host'  by listening 
 * to  ICMP  packets with  a  specific id
 * (default: 7530). uses stdin and stdout
 * and needs to run as root.
 c *
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "driver.h"

/*
 * FIXME this seems really screwed up. This stuff is supposed to be in the files I'm including above.
 */
void herror(const char *s);

struct icmp {
  u_int8_t	type;
  u_int8_t	code;
  u_int16_t	cksum;
  u_int16_t	id;
  u_int16_t	seq;
};

struct ip {
  unsigned int	ip_hl:4, /* both fields are 4 bits */
    ip_v:4;			
  unsigned char	ip_tos;			
  unsigned short	ip_len;			
  unsigned short	ip_id;			
  unsigned short	ip_off;			
  unsigned char	ip_ttl;			
  unsigned char	ip_p;			
  unsigned short	ip_sum;			
  struct	in_addr ip_src,ip_dst;
};

unsigned short in_cksum(unsigned short *, int);

/* icmp_tunnel - does the ICMP tunneling :-)
   int sock - ICMP socket used to communicate
   int proxy - 0 means send echo requests, 1 means send echo replies
   struct sockaddr_in *target - other side
   int tun_fd - input/output file descriptor
   int packetsize - the size of the buffer to allocate for the data part of the packet, apparently?
   u_int16_t id - tunnel id field, apparently
*/
int icmp_tunnel(int sock, int proxy, struct sockaddr_in *target, int tun_fd, int packetsize, u_int16_t id) {
  char* packet;
  struct icmp *icmp, *icmpr;
  int len;
  int result;
  fd_set fs;

  struct sockaddr_in from;
  int fromlen;
  int num;

  len = sizeof (struct icmp);

  packet = malloc (len+packetsize);
  memset (packet, 0, len+packetsize);

  icmp = (struct icmp*)(packet);
  icmpr = (struct icmp*)(packet+sizeof(struct ip));

  while (1) {
    FD_ZERO (&fs);
    FD_SET (tun_fd, &fs);
    FD_SET (sock, &fs);

    select (tun_fd>sock?tun_fd+1:sock+1, &fs, NULL, NULL, NULL);

    /* data available on tunnel device */
    if (FD_ISSET (tun_fd, &fs)) {
      result = tun_read (tun_fd, packet+len, packetsize);
      if (!result) {
        return 0;
      } else if (result==-1) {
        perror("read");
        return -1;
      }
      icmp->type = proxy ? 0 : 8;/*echo request or echo response*/
      icmp->code = 0;
      icmp->id = id;/*mark the packet so the other end knows we care about it*/
      icmp->seq = 0;
      icmp->cksum = 0;
      icmp->cksum = in_cksum((unsigned short*)packet, len+result);
      result = sendto(sock, (char*)packet, len+result, 0, (struct sockaddr*)target, sizeof (struct sockaddr_in));
      if (result==-1) {
        perror ("sendto");
        return -1;
      }
    }

    /* data available on socket */
    if (FD_ISSET(sock, &fs)) {
      fromlen = sizeof (struct sockaddr_in);
      num = recvfrom(sock, packet, len+packetsize, 0, (struct sockaddr*)&from, (socklen_t*) &fromlen);
      /* the data packet */
      if (icmpr->id == id) {/*this filters out all of the other tunnel packets I don't care about*/
        tun_write(tun_fd, packet+sizeof(struct ip)+sizeof(struct icmp), num-sizeof(struct ip)-sizeof(struct icmp));
        /* one IPv4 client */
        memcpy(&(target->sin_addr.s_addr), &(from.sin_addr.s_addr), 4*sizeof(char));
      }
    }    /* end of data available */
  }  /* end of while(1) */

  return 0;
}

/*
 * this is the function that starts it all rolling
 * id - the id value for the icmp stream, to distinguish it from any other tunnels running?
 * packetsize - I think this is the mtu value for the packets going across the tunnel, seems to be used in buffer allocations
 * argv[1] - should be either "-s" or "-c" for server or client mode
 * argv[2] - should be a remote host. this seems to be a requirement regardless of mode
 * tun_fd - the file descriptor of the socket we read and write from
 * FIXME these arguments are retarded
 */
int run_icmp_tunnel (int id, int packetsize, char **argv, int tun_fd) {
  struct sockaddr_in target;
  int s;
  char *daemon = argv[1];
  char *desthost = argv[2];

  if (!desthost) { /*this doesn't make sense for server mode, does it?*/
    fprintf (stderr, "no destination\n");
    return -1;
  }

  if ((target.sin_addr.s_addr = inet_addr (desthost)) == -1) {
    struct hostent* he;
    if (!(he = gethostbyname (desthost))) {
      herror ("gethostbyname");
      return -1;
    }
    memcpy (&target.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
  }
  target.sin_family = AF_INET;

  if ( (s = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
    perror ("socket");
    return -1;
  }

  icmp_tunnel(s, !strcmp(daemon, "-s"), &target, tun_fd, packetsize, (u_int16_t) id);

  close(s);

  return 0;
}

/*
 * calculate the icmp checksum for the packet, including data
 */
unsigned short
in_cksum (unsigned short *addr, int len) {
  register int nleft = len;
  register unsigned short *w = addr;
  register int sum = 0;
  unsigned short answer = 0;
  while (nleft > 1) {
    sum += *w++; nleft -= 2;
  }
  if (nleft == 1) {
    *(unsigned char *) (&answer) = *(unsigned char *) w; sum += answer;
  }
  sum = (sum >> 16) + (sum & 0xffff);   /* add hi 16 to low 16 */
  sum += (sum >> 16);           /* add carry */
  answer = ~sum;                /* truncate to 16 bits */
  return (answer);
}
/*
 * General Purpose AppleTalk Packet Filter Interface
 *
 * Copyright (c) 1992-1995, The University of Melbourne.
 * All Rights Reserved.  Permission to redistribute or
 * use any part of this software for any purpose must
 * be obtained in writing from the copyright owner.
 *
 * This software is supplied "as is" without express
 * or implied warranty.
 *
 * djh@munnari.OZ.AU
 *
 * Supports:
 *	Linux SOCK_PACKET
 *	
 * $Author: maja $
 * $Revision: 1.1 $
 *
 *
 * Modified for use with the linux-mopd port by Karl Maftoum 
 * u963870@student.canberra.edu.au
 *
 */

/*
 * include header files
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/errno.h>
#include <linux/if_ether.h>
#include <netdb.h>
#include <ctype.h>
#include <string.h>

#define MOPDEF_SUPRESS_EXTERN
#include "common/mopdef.h"

/*
 * definitions
 *
 */

#define	READBUFSIZ	4096
#define	NUMRDS		32

struct RDS {
  u_short dataLen;
  u_char *dataPtr;
};

/*
 * variables
 *
 */

struct socklist {
  int iflen;
  struct sockaddr sa;
} socklist[32];

struct ifreq ifr;
extern int errno;
extern int promisc;

struct RDS RDS[NUMRDS];

/*
 * Open and initialize packet filter
 * for a particular protocol type.
 *
 */


int
pfInit(interface, mode, protocol, typ)
char *interface;
u_short protocol;
int typ, mode;
{
  int s;
  int ioarg;
  char device[64];
  unsigned long if_flags;


  { u_short prot;

    prot = ((typ == TRANS_8023) ? htons(ETH_P_802_2) : htons(protocol));
    if ((s = socket(AF_INET, SOCK_PACKET, prot)) < 0) {
      perror(interface);
      return(-1);
    }
    if (s >= 32) {
      close(s);
      return(-1);
    }
  }

  /*
   * set filter for protocol and type (IPTalk, Phase 1/2)
   *
   */

  if (setup_pf(s, protocol, typ) < 0)
    return(-1);

  /*
   * set options, bind to underlying interface
   *
   */

  strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name));

  /* record socket interface name and length */
  strncpy(socklist[s].sa.sa_data, interface, sizeof(socklist[s].sa.sa_data));
  socklist[s].iflen = strlen(interface);

  return(s);
}

/*
 * establish protocol filter
 *
 */

int
setup_pf(s, prot, typ)
int s, typ;
u_short prot;
{
  int ioarg;
  u_short offset;
  return(0);
}

/*
 * get the interface ethernet address
 *
 */

int
pfEthAddr(s, interface, addr)
int s;
char *interface;
u_char *addr;
{
  strcpy(ifr.ifr_name, interface);
  ifr.ifr_addr.sa_family = AF_INET;
  if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
    perror("SIOCGIFHWADDR");
    return(-1);
  }
  memcpy((char *)addr, ifr.ifr_hwaddr.sa_data, 6);
  return(0);
}

/*
 * add a multicast address to the interface
 *
 */

int
pfAddMulti(s, interface, addr)
int s;
char *interface;
u_char *addr;
{
  int sock;

  strcpy(ifr.ifr_name, interface);

  ifr.ifr_addr.sa_family = AF_UNSPEC;
  bcopy((char *)addr, ifr.ifr_addr.sa_data, 6);

  /*
   * open a socket, temporarily, to use for SIOC* ioctls
   *
   */
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return(-1);
  }
  if (ioctl(sock, SIOCADDMULTI, (caddr_t)&ifr) < 0) {
    perror("SIOCADDMULTI");
    close(sock);
    return(-1);
  }
  close(sock);
  
  return(0);
}

/*
 * delete a multicast address from the interface
 *
 */

int
pfDelMulti(s, interface, addr)
int s;
char *interface;
u_char *addr;
{
  int sock;

  strcpy(ifr.ifr_name, interface);

  ifr.ifr_addr.sa_family = AF_UNSPEC;
  bcopy((char *)addr, ifr.ifr_addr.sa_data, 6);

  /*
   * open a socket, temporarily, to use for SIOC* ioctls
   *
   */
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return(-1);
  }
  if (ioctl(sock, SIOCDELMULTI, (caddr_t)&ifr) < 0) {
    perror("SIOCDELMULTI");
    close(sock);
    return(-1);
  }
  close(sock);

  return(0);
}

/*
 * return 1 if ethernet interface capable of multiple opens
 *
 */

int
eth_mopen(phase)
int phase;
{
  if (phase == 2)
    return(0);
  return(1);
}

/*
 * read a packet
 * Read Data Structure describes packet(s) received
 *
 */




int
pfRead(fd, buf, len)
int fd, len;
u_char *buf;
{
  int i, cc;

  int fromlen;
  struct sockaddr sa;

  RDS[0].dataLen = 0;
  fromlen = sizeof(struct sockaddr);

  if ((cc = recvfrom(fd, (char *)buf, len, 0, &sa, &fromlen)) <= 0)
    return(cc);

  /* check if from right interface */
  for (i = socklist[fd].iflen-1; i >= 0; i--)
    if (sa.sa_data[i] != socklist[fd].sa.sa_data[i])
      return(0);

  RDS[0].dataLen = cc;
  RDS[0].dataPtr = buf;
  RDS[1].dataLen = 0;

  return(cc);
}

/*
 * write a packet
 *
 */

int
pfWrite(fd, buf, len)
int fd, len;
u_char *buf;
{

  if (sendto(fd, buf, len, 0, &socklist[fd].sa, sizeof(struct sockaddr)) == len)
    return(len);

  return(-1);
}

/*
 * Return information to device.c how to open device.
 * In this case the driver can handle both Ethernet type II and
 * IEEE 802.3 frames (SNAP) in a single pfOpen.
 */

int
pfTrans(interface)
	char *interface;
{
	return TRANS_ETHER+TRANS_8023;
}


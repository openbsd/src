/*	$NetBSD: tftp.c,v 1.4 1996/02/02 18:06:23 mycroft Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Trivial File Transfer Protocol (see RFC 783).
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */

#include "proto.h"
#include "assert.h"
#include <sys/param.h>
#include "packet.h"
#include "ether.h"
#include "inet.h"
#include "tftp.h"
#include "arp.h"

ipaddr_t tftp_server;	/* IP address of TFTP server */
ipaddr_t tftp_gateway;
static char tftp_file_name[100];
static short block;		/* current block */
static int ctid, stid;		/* UDP client and server TID (network order) */

extern u_long work_area_org;

/*
 * Print IP address in a readable form
 */
void
IpPrintAddr(ipaddr_t addr) {
  inetaddr_t ip;

  ip.a = addr;
  printf("%d.%d.%d.%d", ip.s.a0, ip.s.a1, ip.s.a2, ip.s.a3);
}

/*
 * Generic TFTP error routine
 */
static void
TftpFail(ipaddr_t fromaddr, ipaddr_t toaddr, char *filename, char *reason) {
  printf("Tftp of file '%s' from ", filename);
  IpPrintAddr(fromaddr);
  printf(" failed, %s\n", reason);
}

/*
 * One complement check sum
 */
static u_short
InChecksum(char *cp, u_long count) {
  u_short *sp;
  u_long sum, oneword = 0x00010000;

  for (sum = 0, sp = (u_short *)cp, count >>= 1; count--; ) {
    sum += *sp++;
    if (sum >= oneword) {
      /* wrap carry into low bit */
      sum -= oneword;
      sum++;
    }
  }
  return ~sum;
}

/*
 * Setup the standard IP header fields for a destination,
 * and send packet (possibly using the gateway).
 */
void
IpSend(packet_t *pkt, ipaddr_t dst, ipaddr_t gateway) {
  iphdr_t *ip;
  u_char edst[ETH_ADDRSIZE];
  static int ipid = 0;
#if TRACE > 0
DUMP_STRUCT("IpSend: pkt (front)", pkt, 100);
#endif
  pkt->pkt_offset -= sizeof(iphdr_t);
  pkt->pkt_len += sizeof(iphdr_t);
  ip = (iphdr_t *) pkt->pkt_offset;
  ip->ip_vhl = (IP_VERSION << 4) | (sizeof(*ip) >> 2);
  ip->ip_tos = 0;
  ip->ip_len = htons(pkt->pkt_len);
  ip->ip_id = ipid++;
  ip->ip_off = 0;
  ip->ip_ttl = IP_FRAGTTL;
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_src = ip_myaddr ? ip_myaddr : IP_ANYADDR;
  ip->ip_dst = dst;
  ip->ip_sum = 0;
  ip->ip_sum = InChecksum((char *)ip, sizeof(*ip));
#if 0
/* DUMP_STRUCT("pkt (after)", pkt, 100); */
DUMP_STRUCT("ip", ip, sizeof(iphdr_t)+pkt->pkt_len);
#endif
  if (ArpResolve(pkt, gateway ? gateway : dst, edst)) {
    EtherSend(pkt, ETHTYPE_IP, edst);
    PktRelease(pkt);
  }
}

/*
 * States which TFTP can be in
 */
enum TftpPacketStatus {
  TFTP_RECD_GOOD_PACKET,
  TFTP_RECD_BAD_PACKET,
  TFTP_RECD_SERVER_ABORT,
};

/*
 * Pseudo header to compute UDP checksum
 */
struct pseudoheader {
  ipaddr_t	ph_src;
  ipaddr_t	ph_dst;
  u_char	ph_zero;
  u_char	ph_prot;
  u_short	ph_length;
};

/*
 * Determine whether this IP packet is the TFTP data packet
 * we were expecting. When a broadcast TFTP request was made
 * we'll set the TFTP server address as well.
 */
static enum TftpPacketStatus
TftpDigestPacket(packet_t *pkt, char *rbuf, u_long *rlen) {
  iphdr_t *ip;
  udphdr_t *up;
  tftphdr_t *tp;
  struct pseudoheader ph;
  u_short oldsum, sum;
  u_short udplength;

  /* check for minimum size tftp packet */
  if (pkt->pkt_len < (sizeof(ethhdr_t) + sizeof(iphdr_t) +
		      sizeof(udphdr_t) + sizeof(tftphdr_t))) {
#if 0
    printe("TftpDigestPacket: bad packet size %d\n", pkt->pkt_len);
#endif
    return TFTP_RECD_BAD_PACKET;
  }

  /* IP related checks */
  ip = (iphdr_t *) (pkt->pkt_offset + sizeof(ethhdr_t));
  if (tftp_server != IP_BCASTADDR && ip->ip_src != tftp_server) {
#if 0
    printe("TftpDigestPacket: incorrect ip source address 0x%x\n", ip->ip_src);
#endif
    return TFTP_RECD_BAD_PACKET;
  }
  if (ntohs(ip->ip_len) <
      sizeof(iphdr_t) + sizeof(udphdr_t) + sizeof(tftphdr_t)) {
#if 0
    printe("TftpDigestPacket: bad ip length %d\n", ip->ip_len);
#endif
    return TFTP_RECD_BAD_PACKET;
  }
  if (ip->ip_p != IP_PROTO_UDP) {
#if 0
    printe("TftpDigestPacket: wrong ip protocol type 0x%x\n", ip->ip_p);
#endif
    return TFTP_RECD_BAD_PACKET;
  }
  if (ip_myaddr && ip->ip_dst != ip_myaddr) {
#if 0
    printe("TftpDigestPacket: incorrect ip destination address %x\n", ip->ip_dst);
#endif
    return TFTP_RECD_BAD_PACKET;
  }

  /* UDP related checks */
  up = (udphdr_t *) ((char *)ip + sizeof(iphdr_t));
  if (block && up->uh_sport != stid) {
#if 0
    printe("TftpDigestPacket: wrong udp source port 0x%x\n", up->uh_sport);
#endif
    return TFTP_RECD_BAD_PACKET;
  }
  *rlen = ntohs(up->uh_len) - sizeof(udphdr_t) - sizeof(tftphdr_t);
  if (up->uh_dport != ctid) {
#if 0
    printe("TftpDigestPacket: wrong udp destination port 0x%x\n", up->uh_dport);
#endif
    return TFTP_RECD_BAD_PACKET;
  }

  /* compute UDP checksum if any */
  oldsum = up->uh_sum;
  if (oldsum) {
    udplength = ntohs(up->uh_len);
    /*
     * zero the byte past the last data byte because the
     * checksum will be over an even number of bytes.
     */
    if (udplength & 01)	
      ((char *)up)[udplength] = '\0';
	
    /* set up the pseudo-header */
    ph.ph_src = ip->ip_src;
    ph.ph_dst = ip->ip_dst;
    ph.ph_zero = 0;
    ph.ph_prot = ip->ip_p;
    ph.ph_length = htons(udplength);

    up->uh_sum = ~InChecksum((char *)&ph, sizeof(ph));
    sum = InChecksum((char *)up, (u_long)((udplength + 1) & ~1));
    up->uh_sum = oldsum; /* put original back */
    if (oldsum == (u_short) -1)
      oldsum = 0;
    if (sum != oldsum) {
#if 0
      printe("TftpDigestPacket: Bad checksum %x != %x, length %d from ",
	     sum, oldsum, udplength);
      IpPrintAddr(ip->ip_src);
      printe("\n");
#endif
      return TFTP_RECD_BAD_PACKET;
    }
  }

  /* TFTP related checks */
  tp = (tftphdr_t *) ((char *)up + sizeof(udphdr_t));
  switch (ntohs(tp->th_op)) {
  case TFTP_ERROR:
    printf("Diagnostic from server: error #%d, %s\n",
	   ntohs(tp->th_code), &tp->th_msg);
    return TFTP_RECD_SERVER_ABORT;
  case TFTP_DATA:
    break;
  default:
#if 0
    printe("TftpDigestPacket: incorrect tftp packet type 0x%x\n", tp->th_op);
#endif
    return TFTP_RECD_BAD_PACKET;
  }

  /* reject old packets */
  if (ntohs(tp->th_block) != block + 1) {
#if 0
    printe("TftpDigestPacket: bad block no. %d\n", tp->th_block);
#endif
    return TFTP_RECD_BAD_PACKET;
  }

  /* some TFTP related check */
  if (block == 0) {
    stid = up->uh_sport;
    /* in case of a broadcast, remember server address */
    if (tftp_server == IP_BCASTADDR) {
      tftp_server = ip->ip_src;
#if 0
      printe("Found TFTP server at ");
      IpPrintAddr(tftp_server);
      printe("\n");
#endif
    }
  }
  if (stid != up->uh_sport) {
#if 0
    printe("TftpDigestPacket: incorrect udp source port 0x%x\n", up->uh_sport);
#endif
    return TFTP_RECD_BAD_PACKET;
  }

  bcopy(&tp->th_data, rbuf, *rlen);

  /* advance to next block */
  block++;
  return TFTP_RECD_GOOD_PACKET;
}

enum TftpStatus {
  TFTP_SUCCESS,
  TFTP_FAILURE,
};

static enum TftpStatus
Tftp(char *rbuf, u_long *rlen) {
  u_long time, current, timeout;
  int retry, quit;
  enum TftpStatus rc = TFTP_FAILURE;

  *rlen = 0;
  timeout = 4; /* four seconds */
  for (retry=0, quit=0; ++retry < NRETRIES && !quit; ) {
    /*
     * Send out a TFTP request. On the first block (actually
     * zero) we send out a read request. Every other block we
     * just acknowledge.
     */
    packet_t *pkt;
    ethhdr_t *ep;
    udphdr_t *up;
    tftphdr_t *tp;
#if TRACE > 0
printe("Tftp: block %d, try #%d\n", block, retry);
#endif
    pkt = PktAlloc(sizeof(ethhdr_t) + sizeof(iphdr_t));
    up = (udphdr_t *) pkt->pkt_offset;
    tp = (tftphdr_t *) (pkt->pkt_offset + sizeof(udphdr_t));
    if (block == 0) { /* <RRQ> | <filename> | 0 | "octet" | 0 */
      char *cp, *p;

      tp->th_op = htons(TFTP_RRQ);
      cp = tp->th_stuff;
      for (p = tftp_file_name; *p; )
	*cp++ = *p++;
      *cp++ = '\0';
      *cp++ = 'o';
      *cp++ = 'c';
      *cp++ = 't';
      *cp++ = 'e';
      *cp++ = 't';
      *cp++ = '\0';
      pkt->pkt_len = sizeof(udphdr_t) + (cp - (char *)tp);
    } else { /* else <ACK> | <block> */
      tp->th_op = htons(TFTP_ACK);
      tp->th_block = htons(block);
#if 0
printe("ack block %x %x\n", tp->th_block, block);
#endif
      pkt->pkt_len = sizeof(udphdr_t) + sizeof(tftphdr_t);
    }
    up->uh_sport = ctid;
    up->uh_dport = stid;
    up->uh_sum = 0;
    up->uh_len = htons(pkt->pkt_len);
#if 0
DUMP_STRUCT("tftphdr_t", tp, sizeof(tftphdr_t));
DUMP_STRUCT("udphdr_t", up, sizeof(udphdr_t));
printe("Tftp: ");
#endif
    IpSend(pkt, tftp_server, tftp_gateway);

    /*
     * Receive TFTP data or ARP packets
     */
    time = timer() + timeout;
    do {
      pkt = EtherReceive();
      if (pkt) {
	static int spin = 0;
	ep = (ethhdr_t *) pkt->pkt_offset;
#if 0
DUMP_STRUCT("ethhdr_t", ep, sizeof(ethhdr_t));
#endif
	switch (ntohs(ep->eth_proto)) {
	case ETHTYPE_ARP:
	  ArpInput(pkt);
	  break;
	case ETHTYPE_IP:
	  switch (TftpDigestPacket(pkt, rbuf, rlen)) {
	  case TFTP_RECD_GOOD_PACKET:
	    if (block % 8 == 0)
	      printf("%c\b", "-\\|/"[spin++ % 4]);
#if 0
DUMP_STRUCT("good tftp packet", pkt, 100);
printe("TBD - copy tftp packet #%d, len %d to buffer\n", block, *rlen);
#endif
	    rc = TFTP_SUCCESS;
	    quit = 1;
	    break;
	  case TFTP_RECD_SERVER_ABORT:
	    TftpFail(tftp_server, ip_myaddr, tftp_file_name, "aborted by server");

	    rc = TFTP_FAILURE;
	    quit = 1;
	    break;
	  default:
	    /* for anything else, retry */
#if 0
printe("Tftp: bogus IP packet rec'd, still waiting\n");
#endif
	    break;
	  }
	  break;
	default:
#if 0
printe("Tftp: undesired ethernet packet (type 0x%x) rec'd, still waiting\n",
       ep->eth_proto);
#endif
	  break;
	}
	PktRelease(pkt);
      }
      current = timer();
      HandleKbdAttn();
    } while (current < time && !quit);

#if 0
/* TBD - move */
    eth_reset();
#endif

    if (current >= time)
      timeout <<= 1;
  }

  if (retry > NRETRIES) {
    TftpFail(tftp_server, ip_myaddr, tftp_file_name, "timed Out");
  }
  return rc;
}

static int tftp_at_eof = 1;
static u_long tftp_unread_bytes_in_buffer = 0;

void
SetTftpParms(ipaddr_t server, ipaddr_t gateway, char *file_name) {
  block = 0;
  strncpy(tftp_file_name, file_name, MAX_FILE_NAME_LEN);
  tftp_server = server;
  tftp_at_eof = 0;
  tftp_unread_bytes_in_buffer = 0;
  stid = htons(IP_PORT_TFTP);
  ctid = htons(rand());
  printf("Attempting to tftp file '%s'", tftp_file_name);
  if (tftp_server != IP_BCASTADDR) {
    printf(" from server ");
    IpPrintAddr(tftp_server);
  } else
    printf(" using IP broadcast");
  tftp_gateway = gateway;
  if (tftp_gateway) {
    printf(" using gateway ");
    IpPrintAddr(tftp_gateway);
  }
  printf("\n");
}

u_long
Read(void *result, u_long n_req) {
  static u_long bufp = 0;
  static char buf[PKT_DATASIZE];
  u_long length;
  u_long n_recd = 0;
  while (n_req && !tftp_at_eof) {
    if (tftp_unread_bytes_in_buffer) {
      *((char *)result)++ = buf[bufp++];
      n_req--;
      n_recd++;
      tftp_unread_bytes_in_buffer--;
    } else {
      switch (Tftp(buf, &length)) {
      case TFTP_SUCCESS:
	tftp_unread_bytes_in_buffer = length;
	bufp = 0;
	if (length < SEGSIZE)
	  tftp_at_eof = 1;
	break;
      default:
	/* anything else should cause this to abend */
	tftp_unread_bytes_in_buffer = 0;
	tftp_at_eof = 1;
	break;
      }
    }
  }
  return n_recd;
}

u_long
PhysRead(u_long addr, u_long n_req) {
  u_long n_recd = 0;
  while (n_req) {
    char buf[512];
    u_long nd = n_req<sizeof(buf) ? n_req : sizeof(buf);
    u_long nr = Read(buf, nd);
    if (nr == 0) {
      /* problem, incomplete read */
      break;
    }
    PhysBcopy(LA(buf), addr, nr);
    n_req -= nr;
    n_recd += nr;
    addr += nr;
  }
  return n_recd;
}

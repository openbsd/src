/*	$NetBSD: arp.c,v 1.5 1996/02/02 18:06:14 mycroft Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Ethernet (Reverse) Address Resolution Protocol (see RFC 903, and 826).
 * No doubt this code is overkill, but I had it lying around.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */

#include "proto.h"
#include "assert.h"
#include <sys/param.h>
#include "packet.h"
#include "ether.h"
#include "inet.h"
#include "arp.h"
#include "bootp.h"
#include "tftp.h"

static u_char bcastaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static arptab_t arptab[ARPTAB_SIZE];

extern u_char vendor_area[64];
ipaddr_t ip_myaddr = IP_ANYADDR;
ipaddr_t ip_gateway = IP_ANYADDR;

#ifdef USE_RARP
/*
 * Broadcast a RARP request (i.e. who knows who I am)
 */
static void
RarpWhoAmI(void) {
  arphdr_t *ap;
  packet_t *pkt;
  pkt = PktAlloc(sizeof(ethhdr_t));
  pkt->pkt_len = sizeof(arphdr_t);
  ap = (arphdr_t *) pkt->pkt_offset;
  ap->arp_hrd = htons(ARPHRD_ETHER);
  ap->arp_pro = htons(ETHTYPE_IP);
  ap->arp_hln = ETH_ADDRSIZE;
  ap->arp_pln = sizeof(ipaddr_t);
  ap->arp_op = htons(REVARP_REQUEST);
  bcopy((char *)eth_myaddr, (char *)ap->arp_sha, ETH_ADDRSIZE);
  bcopy((char *)eth_myaddr, (char *)ap->arp_tha, ETH_ADDRSIZE);
  EtherSend(pkt, ETHTYPE_RARP, bcastaddr);
  PktRelease(pkt);
}
#endif


#ifdef USE_BOOTP
static int saved_bootp_xid; /* from last bootp req */
extern int time_zero;
/*
 * Broadcast a BOOTP request (i.e. who knows who I am)
 */
static void
BootpWhoAmI(void) {
  struct bootp *bp;
  packet_t *pkt;
  udphdr_t *up;
  pkt = PktAlloc(sizeof(ethhdr_t)+sizeof(iphdr_t));
  pkt->pkt_len = sizeof(ethhdr_t) + sizeof(iphdr_t) +
    sizeof(udphdr_t) +sizeof(struct bootp);
  up = (udphdr_t *) pkt->pkt_offset;
  bp = (struct bootp *) ((char *)up + sizeof(udphdr_t));
  up->uh_dport = htons(IPPORT_BOOTPS);
  up->uh_len = htons(sizeof(udphdr_t) + sizeof(struct bootp));
  bp->bp_op = BOOTREQUEST;
  bp->bp_htype = 1;
  bp->bp_hlen = ETH_ADDRSIZE;
  bp->bp_xid = saved_bootp_xid = rand();
  bp->bp_secs = htons(timer() - time_zero);
  bcopy((char *)eth_myaddr, (char *)bp->bp_chaddr, ETH_ADDRSIZE);
  IpSend(pkt, IP_BCASTADDR, IP_ANYADDR);
  PktInit();
}
#endif

extern ipaddr_t tftp_gateway;
extern ipaddr_t tftp_server;

#ifdef USE_RARP
/*
 * Called when packet containing RARP is received
 */
static inline ipaddr_t
RarpInput(packet_t *pkt, ipaddr_t *server) {
  ipaddr_t ipaddr;
  ethhdr_t *ep;
  ep = (ethhdr_t *)pkt->pkt_offset;

  /* is rarp? */
  if (pkt->pkt_len >= sizeof(arphdr_t) &&
      ntohs(ep->eth_proto) == ETHTYPE_RARP) {
    ipaddr_t ipa;
    arphdr_t *ap;
    ap = (arphdr_t *) (pkt->pkt_offset + sizeof(ethhdr_t));
    if (ntohs(ap->arp_op) != REVARP_REPLY ||
	ntohs(ap->arp_pro) != ETHTYPE_IP)
      return 0;
    if (bcmp(ap->arp_tha, eth_myaddr, ETH_ADDRSIZE))
      return 0;

    bcopy((char *)ap->arp_tpa, (char *)&ipaddr, sizeof(ipaddr_t));
    printf("From RARP server ");
    bcopy((char *)ap->arp_spa, (char *)&ipa, sizeof(ipaddr_t));
    IpPrintAddr(ipa);
    printf(": using IP address ");
    IpPrintAddr(ipaddr);

    if (server) {
      bcopy((char *)ap->arp_spa, (char *)server, sizeof(ipaddr_t));
      printf(",\n tftp server ");
      IpPrintAddr(*server);
    }

    printf("\n");
    return ipaddr;
  }
  return 0;
}
#endif

#ifdef USE_BOOTP
static inline ipaddr_t
BootpInput(packet_t *pkt, ipaddr_t *server, ipaddr_t *gateway, char *filename) {
  ipaddr_t ipaddr;
  ethhdr_t *ep;
  ep = (ethhdr_t *)pkt->pkt_offset;


  if (pkt->pkt_len < sizeof(iphdr_t)+sizeof(udphdr_t)+sizeof(struct bootp))
    return 0;
  if (ntohs(ep->eth_proto) == ETHTYPE_IP) {
    iphdr_t *ip;
    udphdr_t *up;
    struct bootp *bp;
    ip = (iphdr_t *) ((char *)ep + sizeof(ethhdr_t));
    up = (udphdr_t *) ((char *)ip + sizeof(iphdr_t));
    bp = (struct bootp *) ((char *)up + sizeof(udphdr_t));

#if 0
DUMP_STRUCT("eboot", ep, 100);
printf("pktlen %d of %d\n\n", pkt->pkt_len, sizeof(iphdr_t)+sizeof(udphdr_t)+sizeof(struct bootp));
#endif

    if (ip->ip_p != IP_PROTO_UDP) {
      return 0;
    }

    if (up->uh_dport != htons(IPPORT_BOOTPC)) {
      return 0;
    }

    if (bp->bp_xid != saved_bootp_xid) {
      return 0;
    }

    /* passed all checks - is the packet we expected */
    ipaddr = bp->bp_yiaddr;
    printf("From BOOTP server ");
    IpPrintAddr(ip->ip_src);
    printf(": using IP address ");
    IpPrintAddr(bp->bp_yiaddr);

    if (server) {
      *server = bp->bp_siaddr;
      printf(",\n tftp server ");
      IpPrintAddr(bp->bp_siaddr);
    }

    if (bp->bp_giaddr) {
      *gateway = bp->bp_giaddr;
      printf(",\n gateway ");
      IpPrintAddr(bp->bp_giaddr);
    }

    if (*bp->bp_file) {
      bcopy((char *)bp->bp_file, filename, MAX_FILE_NAME_LEN-1);
      printf(",\n file '%s'", bp->bp_file);
    }

    bcopy((char *)bp->bp_vend, (char *)vendor_area, sizeof(vendor_area));

    printf("\n");

    PktInit();
    return ipaddr;
  }
  return 0;
}
#endif

/*
 * Using the BOOTP and/or RARP request/reply exchange we try to obtain our
 * internet address (see RFC 903).
 */
ipaddr_t
GetIpAddress(ipaddr_t *serv_addr, ipaddr_t *myaddr, ipaddr_t *gateway, char *filename) {
  u_long time, current, timeout;
  int retry;
  packet_t *pkt;
  int spin = 0;

#if TRACE > 0
  printe("GetIpAddress: Requesting IP address for ");
  EtherPrintAddr(eth_myaddr);
  printe("\n");
#endif

  timeout = 4; /* four seconds */
  for (retry = 0; retry < NRETRIES; retry++) {
#ifdef USE_RARP
    RarpWhoAmI();
#endif
#ifdef USE_BOOTP
    BootpWhoAmI();
#endif
    printf("%c\b", "-\\|/"[spin++ % 4]);

    time = timer() + timeout;
    do {
      pkt = EtherReceive();
      if (pkt) {
	*myaddr = 0;
#ifdef USE_RARP
	*myaddr = RarpInput(pkt, serv_addr);
#endif
#ifdef USE_BOOTP
	if (!*myaddr)
	  *myaddr = BootpInput(pkt, serv_addr, gateway, filename);
#endif
	PktRelease(pkt);
	if (*myaddr) {
	  return 1;
	}
      }
      HandleKbdAttn();
      current = timer();
    } while (current < time);
    EtherReset();
    timeout <<= 1;
  }
  printf("No response for "
#ifdef USE_BOOTP
	 "BOOTP "
#endif
#ifdef USE_RARP
	 "RARP "
#endif
	 "request\n");
  return IP_ANYADDR;
}

/*
 * Broadcast an ARP packet (i.e. ask who has address "addr")
 */
static void
ArpWhoHas(ipaddr_t addr) {
  arphdr_t *ap;
  packet_t *pkt;

  pkt = PktAlloc(sizeof(ethhdr_t));
  pkt->pkt_len = sizeof(arphdr_t);
  ap = (arphdr_t *) pkt->pkt_offset;
  ap->arp_hrd = htons(ARPHRD_ETHER);
  ap->arp_pro = htons(ETHTYPE_IP);
  ap->arp_hln = ETH_ADDRSIZE;
  ap->arp_pln = sizeof(ipaddr_t);
  ap->arp_op = htons(ARPOP_REQUEST);
  bcopy((char *)eth_myaddr, (char *)ap->arp_sha, ETH_ADDRSIZE);
  bcopy((char *)&ip_myaddr, (char *)ap->arp_spa, sizeof(ipaddr_t));
  bcopy((char *)&addr, (char *)ap->arp_tpa, sizeof(ipaddr_t));
#if TRACE > 0
printe("ArpWhoHas:\n");	
DUMP_STRUCT("arphdr_t", ap, sizeof(arphdr_t));
#endif
  EtherSend(pkt, ETHTYPE_ARP, bcastaddr);
  PktRelease(pkt);
}

/*
 * Free an arptab entry
 */
static void
ArpTfree(arptab_t *at) {
  if (at->at_hold)
    PktRelease(at->at_hold);
  at->at_hold = (packet_t *)0;
  at->at_timer = at->at_flags = 0;
  at->at_ipaddr = 0;
}

/*
 * Enter a new address in arptab, pushing out the oldest entry 
 * from the bucket if there is no room.
 */
static arptab_t *
ArpTnew(ipaddr_t addr) {
  u_short n;
  u_long oldest;
  arptab_t *at, *ato;

  oldest = ~0;
  ato = at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ];
  for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) {
    if (at->at_flags == 0)
      goto out;	 /* found an empty entry */
    if (at->at_timer < oldest) {
      oldest = at->at_timer;
      ato = at;
    }
  }
  at = ato;
  ArpTfree(at);
 out:
  at->at_ipaddr = addr;
  at->at_flags = ATF_INUSE;
  return at;
}

/*
 * Resolve an IP address into a hardware address.  If success, 
 * destha is filled in and 1 is returned.  If there is no entry
 * in arptab, set one up and broadcast a request 
 * for the IP address;  return 0.  Hold onto this packet and 
 * resend it once the address is finally resolved.
 */
int
ArpResolve(packet_t *pkt, ipaddr_t destip, u_char *destha) {
  arptab_t *at;
  u_long lna = ntohl(destip) & 0xFF;

  if (lna == 0xFF || lna == 0x0) { /* broadcast address */
    bcopy((char *)bcastaddr, (char *)destha, ETH_ADDRSIZE);
    return 1;
  }

  ARPTAB_LOOK(at, destip);
  if (at == 0) {
    at = ArpTnew(destip);
    at->at_hold = pkt;
    ArpWhoHas(destip);
    return 0;
  }

  at->at_timer = timer(); /* restart the timer */
  if (at->at_flags & ATF_COM) { /* entry is complete */
    bcopy((char *)at->at_eaddr, (char *)destha, ETH_ADDRSIZE);
    return 1;
  }

  /*
   * There is an arptab entry, but no hardware address
   * response yet.  Replace the held packet with this
   * latest one.
   */
  if (at->at_hold)
    PktRelease(at->at_hold);
  at->at_hold = pkt;
  ArpWhoHas(destip);
  return 0;
}


/*
 * Called when packet containing ARP is received.
 * Algorithm is that given in RFC 826.
 */
void
ArpInput(packet_t *pkt) {
  arphdr_t *ap;
  arptab_t *at;
  packet_t *phold;
  ipaddr_t isaddr, itaddr;

#if 0
T(ArpInput);
#endif
  if (pkt->pkt_len < sizeof(arphdr_t)) {
#if 0
    printf("ArpInput: bad packet size %d\n", pkt->pkt_len);
#endif
    return;
  }

  ap = (arphdr_t *) (pkt->pkt_offset + sizeof(ethhdr_t));
#if 0
DUMP_STRUCT("arphdr_t", ap, sizeof(arphdr_t));
#endif
  if (ntohs(ap->arp_pro) != ETHTYPE_IP) {
#if 0
    printf("ArpInput: incorrect proto addr %x\n", ap->arp_pro);
#endif
    return;
  }

  bcopy((char *)ap->arp_spa, (char *)&isaddr, sizeof(ipaddr_t));
  bcopy((char *)ap->arp_tpa, (char *)&itaddr, sizeof(ipaddr_t));
  if (!bcmp(ap->arp_sha, eth_myaddr, ETH_ADDRSIZE)) {
#if 0
    printf("ArpInput: incorrect sender h/w addr ");
    EtherPrintAddr(ap->arp_sha);
    printf("/n");
#endif
    return;
  }

  at = (arptab_t *)0;
  ARPTAB_LOOK(at, isaddr);
  if (at) {
    bcopy((char *)ap->arp_sha, (char *)at->at_eaddr, ETH_ADDRSIZE);
    at->at_flags |= ATF_COM;
    if (at->at_hold) {
      phold = at->at_hold;
      at->at_hold = (packet_t *)0;
#if 0
      printf("ArpInput: found addr, releasing packet\n");
#endif
      EtherSend(phold, ETHTYPE_IP, at->at_eaddr);
      PktRelease(phold);
    }
  }

  /*
   * Only answer ARP request which are for me
   */
  if (itaddr != ip_myaddr) {
#if 0
    printf("ArpInput: it addr ");
    IpPrintAddr(itaddr);
    printf(" somebody else\n");
#endif
    return;
  }

  if (at == 0) {		/* ensure we have a table entry */
    at = ArpTnew(isaddr);
    bcopy((char *)ap->arp_sha, (char *)at->at_eaddr, ETH_ADDRSIZE);
    at->at_flags |= ATF_COM;
  }
  if (ntohs(ap->arp_op) != ARPOP_REQUEST) {
    printf("ArpInput: incorrect operation: 0x%x\n", ntohs(ap->arp_op));
    return;
  }
  bcopy((char *)ap->arp_sha, (char *)ap->arp_tha, ETH_ADDRSIZE);
  bcopy((char *)ap->arp_spa, (char *)ap->arp_tpa, sizeof(ipaddr_t));
  bcopy((char *)eth_myaddr, (char *)ap->arp_sha, ETH_ADDRSIZE);
  bcopy((char *)&itaddr, (char *)ap->arp_spa, sizeof(ipaddr_t));
  ap->arp_op = htons(ARPOP_REPLY);
#if 0
printf("ArpInput: valid request rec'd, replying\n");
#endif
  EtherSend(pkt, ETHTYPE_ARP, ap->arp_tha);
}

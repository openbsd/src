/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/

#include <sys/osdep.h>

#ifdef OSDEP_BSD
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_icmp.h>
#endif /* OSDEP_BSD */

#include <sys/debug.h>

/*
 * Globals
 */

/*----------------------------------------------------------------------
 * Dump an IPv6 address.  Don't compress 0's out because of debugging.
 ----------------------------------------------------------------------*/
void dump_in6_addr(struct in6_addr *in6_addr)
{
  uint16_t *p = (uint16_t *)in6_addr;
  int i = 0;

  if (!in6_addr) {
    printf("Dereference a NULL in6_addr? I don't think so.\n");
    return;
  }

  while (i++ < 7)
    printf("%04x:", ntohs(*(p++)));
  printf("%04x\n", ntohs(*p));
}

/*----------------------------------------------------------------------
 * Dump an IPv6 socket address.
 ----------------------------------------------------------------------*/
void dump_sockaddr_in6(struct sockaddr_in6 *sin6)
{
  printf("sockaddr_in6 at %08x: ", OSDEP_PCAST(sin6));
  if (!sin6)
    goto ret;

#if OSDEP_SALEN
  printf("len=%d, ", sin6->sin6_len);
#endif /* OSDEP_SALEN */
  printf("family=%d, port=%d, flowinfo=%lx, addr=", sin6->sin6_family, ntohs(sin6->sin6_port), (unsigned long)ntohl(sin6->sin6_flowinfo)); 
  dump_in6_addr(&sin6->sin6_addr);

ret:
  printf("\n");
};

#ifdef OSDEP_BSD
/*----------------------------------------------------------------------
 * Dump an IPv6 header.
 ----------------------------------------------------------------------*/
void dump_ipv6(struct ipv6 *ipv6)
{
  if (!ipv6) {
	printf("Dereference a NULL ipv6? I don't think so.\n");
        return;
  }

  printf("Vers & flow label (conv to host order) 0x%x\n",
	 (unsigned int)htonl(ipv6->ipv6_versfl));
  printf("Length (conv) = %d, nexthdr = %d, hoplimit = %d.\n",
	 htons(ipv6->ipv6_length),ipv6->ipv6_nexthdr,ipv6->ipv6_hoplimit);
  printf("Src: ");
  dump_in6_addr(&ipv6->ipv6_src);
  printf("Dst: ");
  dump_in6_addr(&ipv6->ipv6_dst);
}

/*----------------------------------------------------------------------
 * Dump an ICMPv6 header.  This function is not very smart beyond the
 * type, code, and checksum.
 ----------------------------------------------------------------------*/
void dump_ipv6_icmp(struct ipv6_icmp *icp)
{
  if (!icp) {
	printf("Dereference a NULL ipv6_icmp? I don't think so.\n");
        return;
  }

  printf("type %d, code %d, cksum (conv) = 0x%x\n",icp->icmp_type,
	 icp->icmp_code,htons(icp->icmp_cksum));
  printf("First four bytes: 0x%x", (unsigned int)htonl(icp->icmp_unused));
  printf("Next four bytes: 0x");
  debug_dump_buf((void *)icp->icmp_echodata, 4);
  printf("\n");
}

#ifdef KERNEL
/*----------------------------------------------------------------------
 * Dump an IPv6 discovery queue structure.
 ----------------------------------------------------------------------*/
void dump_discq(struct discq *dq)
{
  if (!dq) {
    printf("Dereference a NULL discq? I don't think so.\n");
    return;
  }

  printf("dq_next = 0x%lx, dq_prev = 0x%lx, dq_rt = 0x%lx,\n", (unsigned long)dq->dq_next,
	 (unsigned long)dq->dq_prev, (unsigned long)dq->dq_rt);
  printf("dq_queue = 0x%lx.\n", (unsigned long)dq->dq_queue);
  /* Dump first mbuf chain? */
  /*printf("dq_expire = %d (0x%x).\n",dq->dq_expire,dq->dq_expire);*/
}
#endif /* KERNEL */
#endif /* OSDEP_BSD */

/*	$NetBSD: arp.h,v 1.3 1994/10/27 04:21:03 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Ethernet Address Resolution Protocol (see RFC 826)
 */

/*
 * ARP packets are variable in size; the arphdr_t type defines the
 * 10Mb Ethernet variant.  Field names used correspond to RFC 826.
 */
typedef struct {
    u_short	arp_hrd;		/* format of hardware address */
#define	ARPHRD_ETHER	1       	/* ethernet hardware address */
    u_short	arp_pro;		/* format of proto. address  */
    u_char	arp_hln;		/* length of hardware address  */
    u_char	arp_pln;		/* length of protocol address  */
    u_short 	arp_op;
#define	ARPOP_REQUEST	1		/* request to resolve address */
#define	ARPOP_REPLY	2		/* response to previous request */
#define	REVARP_REQUEST	3		/* reverse ARP request */
#define	REVARP_REPLY	4		/* reverse ARP reply */
    u_char	arp_sha[ETH_ADDRSIZE];	/* sender hardware address */
    u_char	arp_spa[4];		/* sender protocol address */
    u_char	arp_tha[ETH_ADDRSIZE];	/* target hardware address */
    u_char	arp_tpa[4];		/* target protocol address */
} arphdr_t;

/*
 * Internet to hardware address resolution table
 */
typedef struct {
    ipaddr_t	at_ipaddr;		/* internet address */
    u_char	at_eaddr[ETH_ADDRSIZE];	/* ethernet address */
    u_long	at_timer;		/* time when referenced */
    u_char	at_flags;		/* flags */
    packet_t	*at_hold;		/* ast packet until resolved/timeout */
} arptab_t;

/* at_flags field values */
#define	ATF_INUSE	1		/* entry in use */
#define ATF_COM		2		/* completed entry (eaddr valid) */

#define	ARPTAB_BSIZ	3		/* bucket size */
#define	ARPTAB_NB	2		/* number of buckets */
#define	ARPTAB_SIZE	(ARPTAB_BSIZ * ARPTAB_NB)

#define	ARPTAB_HASH(a) \
	((short)((((a) >> 16) ^ (a)) & 0x7fff) % ARPTAB_NB)

#define	ARPTAB_LOOK(at, addr) { \
	register n; \
	at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ]; \
	for (n = 0; n < ARPTAB_BSIZ; n++, at++) \
		if (at->at_ipaddr == addr) \
			break; \
	if (n >= ARPTAB_BSIZ) \
		at = 0; }

ipaddr_t GetIpAddress(ipaddr_t *server, ipaddr_t *my_addr, ipaddr_t *gateway, char *filename);
void ArpInput(packet_t *);
int ArpResolve(packet_t *, ipaddr_t, u_char *);

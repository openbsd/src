/*	$NetBSD: ether.h,v 1.3 1994/10/27 04:21:12 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * Ethernet definitions
 */

#define	ETH_ADDRSIZE	6		/* address size */

/*
 * Structure of an ethernet header
 */
typedef struct {
  u_char eth_dst[ETH_ADDRSIZE];	/* destination address */
  u_char eth_src[ETH_ADDRSIZE];	/* source address */
  u_short eth_proto;			/* protocol type */
} ethhdr_t;

/* protocol types */
#define	ETHTYPE_IP	0x0800		/* IP protocol */
#define	ETHTYPE_ARP	0x0806		/* ARP protocol */
#define	ETHTYPE_RARP	0x8035		/* Reverse ARP protocol */

extern u_char eth_myaddr[];

int EtherInit(void);
void EtherReset(void);
void EtherStop(void);
void EtherSend(packet_t *pkt, u_short proto, u_char *dest);
packet_t *EtherReceive(void);
void EtherPrintAddr(u_char *addr);

/* TBD - move these elsewhere? */

static inline u_short
htons(u_short x) {
  return ((x >> 8) & 0xff)
    | ((x & 0xff) << 8);
}

#if 0
static inline u_short
ntohs(u_short x) {
  return x >> 8 & 0xff
    | (x & 0xff) << 8;
}
#else
static inline u_short
ntohs(u_short x) {
  return htons(x);
}
#endif

static inline u_long
htonl(u_long x) {
  return (x >> 24 & 0xffL)
    | (x >> 8 & 0xff00L)
      | (x << 8 & 0xff0000L)
	| (x << 24 & 0xff000000L);
}

#if 0
static inline u_long
ntohl(u_long x) {
  return x >> 24 & 0xffL
    | x >> 8 & 0xff00L
      | x << 8 & 0xff0000L
	| x << 24 & 0xff000000L;
}
#else
static inline u_long
ntohl(u_long x) {
  return htonl(x);
}
#endif
